#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>
#include <cmath>
#include <cstring>
#include "logger.h"

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <windows.h>
    #define TITAN_EXPORT __declspec(dllexport)
#else
    #include <dlfcn.h>
    #include <cstdint>
    #define TITAN_EXPORT __attribute__((visibility("default")))
    typedef void* HMODULE;
    #define LoadLibraryA(path) dlopen(path, RTLD_LAZY)
    #define GetProcAddress dlsym
    #define FreeLibrary dlclose
#endif

struct EffectParams {
    std::string name;
    float intensity = 1.0f;
};

typedef const char* (*GetNameFunc)();
// SIGNATURE UPDATED: Requires time parameter
typedef void (*ProcessFunc)(uint8_t* pixels, int width, int height, int pitch, float intensity, float time);

struct ExternalPlugin {
    std::string name;
    HMODULE handle;
    ProcessFunc process;
};

class PluginManager {
public:
    static inline std::vector<ExternalPlugin> externalPlugins;
    static inline std::vector<std::string> internalPlugins = {
        "Black & White", "Glitch / Invert", "Sepia (Retro)", "Directional Blur", "Jiggle"
    };

    static void InitAndScanPlugins() {
        LOG_DEBUG("Initializing PluginManager. Internal plugins loaded: %zu", internalPlugins.size());
        
        std::vector<std::filesystem::path> scanDirs = { ".", "./plugins" };
        
        for (const auto& dir : scanDirs) {
            if (!std::filesystem::exists(dir)) continue;
            for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::string filename = entry.path().filename().string();
                    
                    #ifdef _WIN32
                    bool isPlugin = (ext == ".dll");
                    #else
                    bool isPlugin = (ext == ".so");
                    #endif
                    
                    if (isPlugin && filename != "Editor.dll" && filename != "Editor.so" && filename != "blood_filter.so") {
                        // Avoid infinite loops or loading non-plugin libraries if they don't match
                    }
                    
                    if (isPlugin && filename != "libSDL2.so" && filename != "libSDL2-2.0.so" && 
                        filename != "libavcodec.so" && filename != "libavformat.so" && 
                        filename != "libswscale.so" && filename != "libavutil.so" && 
                        filename != "libswresample.so") {
                        
                        std::string pathStr = entry.path().string();
                        #ifndef _WIN32
                        if (pathStr.find('/') == std::string::npos) {
                            pathStr = "./" + pathStr;
                        }
                        #endif
                        
                        HMODULE handle = LoadLibraryA(pathStr.c_str());
                        if (handle) {
                            GetNameFunc getName = (GetNameFunc)GetProcAddress(handle, "GetPluginName");
                            ProcessFunc process = (ProcessFunc)GetProcAddress(handle, "ProcessFrame");
                            
                            if (getName && process) {
                                ExternalPlugin plugin;
                                plugin.name = getName();
                                plugin.handle = handle;
                                plugin.process = process;
                                externalPlugins.push_back(plugin);
                                LOG_DEBUG("Loaded external plugin: %s from %s", plugin.name.c_str(), pathStr.c_str());
                            } else {
                                FreeLibrary(handle);
                            }
                        }
                    }
                }
            }
        }
    }

    static void Shutdown() {
        for (auto& plugin : externalPlugins) {
            if (plugin.handle) {
                FreeLibrary(plugin.handle);
                plugin.handle = nullptr;
            }
        }
        externalPlugins.clear();
        LOG_DEBUG("PluginManager shutdown: all external plugins unloaded.");
    }

    static std::vector<std::string> GetAvailablePlugins() {
        std::vector<std::string> all = internalPlugins;
        for (const auto& ext : externalPlugins) all.push_back(ext.name);
        return all;
    }

    static void ApplyPlugins(uint8_t* pixels, int width, int height, int pitch, const std::vector<EffectParams>& effects, float currentTime) {
        if (effects.empty() || !pixels) return;

        // 1. Process external plugins
        for (const auto& fx : effects) {
            for (auto& ext : externalPlugins) {
                if (fx.name == ext.name) {
                    ext.process(pixels, width, height, pitch, fx.intensity, currentTime);
                }
            }
        }

        // 2. Multithreaded internal spatial & color processing
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        std::vector<std::thread> threads;
        int chunkHeight = height / numThreads;

        for (int t = 0; t < numThreads; ++t) {
            int startY = t * chunkHeight;
            int endY = (t == numThreads - 1) ? height : startY + chunkHeight;

            threads.emplace_back([=]() {
                // ALLOCATE ONCE PER THREAD. Prevents catastrophic heap fragmentation.
                std::vector<uint8_t> rowBuffer(pitch);

                for (int y = startY; y < endY; ++y) {
                    uint8_t* row = pixels + y * pitch;
                    
                    // Snapshot the unmodified row to prevent read/write race conditions during spatial shifts
                    std::memcpy(rowBuffer.data(), row, pitch);

                    for (int x = 0; x < width; ++x) {
                        int pX = x * 4;
                        int b = row[pX + 0], g = row[pX + 1], r = row[pX + 2];
                        
                        for (const auto& fx : effects) {
                            if (fx.name == "Black & White") {
                                int gray = (r * 299 + g * 587 + b * 114) / 1000;
                                b += (gray - b) * fx.intensity; 
                                g += (gray - g) * fx.intensity; 
                                r += (gray - r) * fx.intensity;
                            }
                            else if (fx.name == "Jiggle") {
                                float shiftX = std::sin(currentTime * 15.0f + y * 0.05f) * 30.0f * fx.intensity;
                                float exactX = x + shiftX;
                                int x0 = static_cast<int>(exactX);
                                int x1 = x0 + 1;
                                float frac = exactX - x0;
                                if (frac < 0.0f) { x0 -= 1; x1 -= 1; frac += 1.0f; }

                                x0 = std::clamp(x0, 0, width - 1);
                                x1 = std::clamp(x1, 0, width - 1);
                                
                                int spX0 = x0 * 4;
                                int spX1 = x1 * 4;
                                
                                b = static_cast<int>(rowBuffer[spX0 + 0] * (1.0f - frac) + rowBuffer[spX1 + 0] * frac);
                                g = static_cast<int>(rowBuffer[spX0 + 1] * (1.0f - frac) + rowBuffer[spX1 + 1] * frac);
                                r = static_cast<int>(rowBuffer[spX0 + 2] * (1.0f - frac) + rowBuffer[spX1 + 2] * frac);
                            }
                            else if (fx.name == "Directional Blur") {
                                int blurRadius = static_cast<int>(fx.intensity * 20.0f);
                                if (blurRadius > 0) {
                                    int sumB = 0, sumG = 0, sumR = 0;
                                    int count = 0;
                                    for (int bx = -blurRadius; bx <= blurRadius; bx += 2) { 
                                        int sx = std::clamp(x + bx, 0, width - 1);
                                        int spx = sx * 4;
                                        sumB += rowBuffer[spx + 0];
                                        sumG += rowBuffer[spx + 1];
                                        sumR += rowBuffer[spx + 2];
                                        count++;
                                    }
                                    b = sumB / count; g = sumG / count; r = sumR / count;
                                }
                            }
                            else if (fx.name == "Glitch / Invert") {
                                r = 255 - r;
                                g = 255 - g;
                                b = 255 - b;
                                // Эффект глитча: смещение каналов + инверсия
                                if ((y + static_cast<int>(currentTime * 60.0f)) % 20 < 3) {
                                    int shift = static_cast<int>(fx.intensity * 40.0f);
                                    int sx = std::clamp(x + shift, 0, width - 1);
                                    int spx = sx * 4;
                                    r = rowBuffer[spx + 2];
                                }
                            }
                            else if (fx.name == "Sepia (Retro)") {
                                int sepiaR = std::clamp(static_cast<int>(r * 0.393f + g * 0.769f + b * 0.189f), 0, 255);
                                int sepiaG = std::clamp(static_cast<int>(r * 0.349f + g * 0.686f + b * 0.168f), 0, 255);
                                int sepiaB = std::clamp(static_cast<int>(r * 0.272f + g * 0.534f + b * 0.131f), 0, 255);
                                r = r + static_cast<int>((sepiaR - r) * fx.intensity);
                                g = g + static_cast<int>((sepiaG - g) * fx.intensity);
                                b = b + static_cast<int>((sepiaB - b) * fx.intensity);
                            }
                        }
                        row[pX + 0] = std::clamp(b, 0, 255);
                        row[pX + 1] = std::clamp(g, 0, 255);
                        row[pX + 2] = std::clamp(r, 0, 255);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
    }
};