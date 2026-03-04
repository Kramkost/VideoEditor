#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <thread>

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #include <windows.h>
    #define TITAN_EXPORT __declspec(dllexport)
#else
    #include <dlfcn.h>
    #include <cstdint>
    #define TITAN_EXPORT __attribute__((visibility("default")))
    // Маппинг Windows-типов для Linux, чтобы не менять остальной код
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
typedef void (*ProcessFunc)(uint8_t* pixels, int width, int height, int pitch, float intensity);

struct ExternalPlugin {
    std::string name;
    HMODULE handle;
    ProcessFunc process;
};

class PluginManager {
public:
    static inline std::vector<ExternalPlugin> externalPlugins;
    static inline std::vector<std::string> internalPlugins = {"Black & White", "Glitch / Invert", "Sepia (Retro)"};

    static void InitAndScanPlugins() {
        externalPlugins.clear();
        if (!std::filesystem::exists("plugins")) std::filesystem::create_directory("plugins");

        std::string ext = 
#ifdef _WIN32
            ".dll";
#else
            ".so";
#endif

        for (const auto& entry : std::filesystem::directory_iterator("plugins")) {
            if (entry.path().extension() == ext) {
                HMODULE hMod = LoadLibraryA(entry.path().string().c_str());
                if (hMod) {
                    GetNameFunc getName = (GetNameFunc)GetProcAddress(hMod, "GetPluginName");
                    ProcessFunc process = (ProcessFunc)GetProcAddress(hMod, "ProcessFrame");
                    
                    if (getName && process) {
                        externalPlugins.push_back({getName(), hMod, process});
                    } else {
                        FreeLibrary(hMod);
                    }
                }
            }
        }
    }

    static std::vector<std::string> GetAvailablePlugins() {
        std::vector<std::string> all = internalPlugins;
        for (auto& p : externalPlugins) all.push_back(p.name);
        return all;
    }

    static void ApplyPlugins(uint8_t* pixels, int width, int height, int pitch, const std::vector<EffectParams>& effects) {
        if (effects.empty()) return;

        for (const auto& fx : effects) {
            for (auto& ext : externalPlugins) {
                if (fx.name == ext.name) ext.process(pixels, width, height, pitch, fx.intensity);
            }
        }

        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        std::vector<std::thread> threads;
        int chunkHeight = height / numThreads;

        for (int t = 0; t < numThreads; ++t) {
            int startY = t * chunkHeight;
            int endY = (t == numThreads - 1) ? height : startY + chunkHeight;

            threads.emplace_back([=]() {
                for (int y = startY; y < endY; ++y) {
                    uint8_t* row = pixels + y * pitch;
                    for (int x = 0; x < width; ++x) {
                        int b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2];
                        for (const auto& fx : effects) {
                            if (fx.name == "Black & White") {
                                int gray = (r * 299 + g * 587 + b * 114) / 1000;
                                b += (gray - b) * fx.intensity; g += (gray - g) * fx.intensity; r += (gray - r) * fx.intensity;
                            }
                            // ... остальные эффекты
                        }
                        row[x * 4 + 0] = std::clamp(b, 0, 255);
                        row[x * 4 + 1] = std::clamp(g, 0, 255);
                        row[x * 4 + 2] = std::clamp(r, 0, 255);
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
    }
};
