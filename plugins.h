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
        // Load external DLLs/SOs here if needed
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
                                int sourceX = std::clamp(x + static_cast<int>(shiftX), 0, width - 1);
                                int spX = sourceX * 4;
                                
                                b = rowBuffer[spX + 0];
                                g = rowBuffer[spX + 1];
                                r = rowBuffer[spX + 2];
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