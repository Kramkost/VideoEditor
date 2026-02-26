#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include <windows.h>
#include <filesystem>
#include <iostream>
#include <thread>

// Настройки наложенного плагина
struct EffectParams {
    std::string name;
    float intensity = 1.0f; // Ползунок интенсивности (0.0 - 1.0)
};

// Сигнатуры функций, которые мы будем искать внутри чужих DLL
typedef const char* (*GetNameFunc)();
typedef void (*ProcessFunc)(uint8_t* pixels, int width, int height, int pitch, float intensity);

// Структура загруженного из папки плагина
struct ExternalPlugin {
    std::string name;
    HMODULE handle;
    ProcessFunc process;
};

class PluginManager {
public:
    static inline std::vector<ExternalPlugin> externalPlugins;
    static inline std::vector<std::string> internalPlugins = {"Black & White", "Glitch / Invert", "Sepia (Retro)"};

    // 1. СКАНЕР ПАПКИ PLUGINS
    static void InitAndScanPlugins() {
        externalPlugins.clear();
        
        // Если папки нет, создаем ее!
        if (!std::filesystem::exists("plugins")) {
            std::filesystem::create_directory("plugins");
        }

        // Ищем все .dll файлы
        for (const auto& entry : std::filesystem::directory_iterator("plugins")) {
            if (entry.path().extension() == ".dll") {
                HMODULE hMod = LoadLibraryA(entry.path().string().c_str());
                if (hMod) {
                    GetNameFunc getName = (GetNameFunc)GetProcAddress(hMod, "GetPluginName");
                    ProcessFunc process = (ProcessFunc)GetProcAddress(hMod, "ProcessFrame");
                    
                    if (getName && process) {
                        externalPlugins.push_back({getName(), hMod, process});
                        std::cout << "SUCCESS: Loaded plugin - " << getName() << "\n";
                    } else {
                        FreeLibrary(hMod);
                        std::cerr << "ERROR: Invalid plugin format in " << entry.path().filename() << "\n";
                    }
                }
            }
        }
    }

    // 2. ОТДАЕМ СПИСОК В UI
    static std::vector<std::string> GetAvailablePlugins() {
        std::vector<std::string> all = internalPlugins;
        for (auto& p : externalPlugins) all.push_back(p.name);
        return all;
    }

    // 3. ПРИМЕНЯЕМ ЭФФЕКТЫ (МНОГОПОТОЧНОСТЬ!)
    static void ApplyPlugins(uint8_t* pixels, int width, int height, int pitch, const std::vector<EffectParams>& effects) {
        if (effects.empty()) return;

        // Сначала отдаем кадр внешним DLL плагинам (они могут делать сложный блюр и т.д.)
        for (const auto& fx : effects) {
            for (auto& ext : externalPlugins) {
                if (fx.name == ext.name) {
                    ext.process(pixels, width, height, pitch, fx.intensity);
                }
            }
        }

        // Узнаем, сколько ядер у процессора (если не удалось, берем 4)
        int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        
        std::vector<std::thread> threads;
        int chunkHeight = height / numThreads;

        // Разрезаем кадр на горизонтальные куски и отдаем каждому ядру процессора!
        for (int t = 0; t < numThreads; ++t) {
            int startY = t * chunkHeight;
            int endY = (t == numThreads - 1) ? height : startY + chunkHeight;

            threads.emplace_back([=]() {
                for (int y = startY; y < endY; ++y) {
                    uint8_t* row = pixels + y * pitch;
                    for (int x = 0; x < width; ++x) {
                        int b = row[x * 4 + 0];
                        int g = row[x * 4 + 1];
                        int r = row[x * 4 + 2];
                        
                        for (const auto& fx : effects) {
                            if (fx.name == "Black & White") {
                                int gray = (r * 299 + g * 587 + b * 114) / 1000;
                                b = b + (gray - b) * fx.intensity;
                                g = g + (gray - g) * fx.intensity;
                                r = r + (gray - r) * fx.intensity;
                            }
                            else if (fx.name == "Glitch / Invert") {
                                b = b + ((255 - b) - b) * fx.intensity;
                                g = g + ((255 - g) - g) * fx.intensity;
                                r = r + ((255 - r) - r) * fx.intensity;
                            }
                            else if (fx.name == "Sepia (Retro)") {
                                int tr = (int)((r * 0.393) + (g * 0.769) + (b * 0.189));
                                int tg = (int)((r * 0.349) + (g * 0.686) + (b * 0.168));
                                int tb = (int)((r * 0.272) + (g * 0.534) + (b * 0.131));
                                r = r + (tr - r) * fx.intensity;
                                g = g + (tg - g) * fx.intensity;
                                b = b + (tb - b) * fx.intensity;
                            }
                        }

                        // Защита от пересвета
                        row[x * 4 + 0] = std::clamp(b, 0, 255);
                        row[x * 4 + 1] = std::clamp(g, 0, 255);
                        row[x * 4 + 2] = std::clamp(r, 0, 255);
                    }
                }
            });
        }

        // Ждем, пока все ядра закончат работу над этим кадром
        for (auto& th : threads) {
            th.join();
        }
    }
};