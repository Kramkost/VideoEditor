#pragma once
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>

// Настройки наложенного плагина
struct EffectParams {
    std::string name;
    float intensity = 1.0f; // Ползунок интенсивности (0.0 - 1.0)
};

class PluginManager {
public:
    // 1. СПИСОК ПЛАГИНОВ (Добавляй сюда новые названия)
    static std::vector<std::string> GetAvailablePlugins() {
        return {"Black & White", "Glitch / Invert", "Sepia (Retro)"};
    }

    // 2. МАТЕМАТИКА ПЛАГИНОВ (Применяется к каждому пикселю видео)
    static void ApplyPlugins(uint8_t* pixels, int width, int height, int pitch, const std::vector<EffectParams>& effects) {
        if (effects.empty()) return;

        for (int y = 0; y < height; ++y) {
            uint8_t* row = pixels + y * pitch;
            for (int x = 0; x < width; ++x) {
                // Читаем цвета пикселя (BGRA формат)
                int b = row[x * 4 + 0];
                int g = row[x * 4 + 1];
                int r = row[x * 4 + 2];
                
                for (const auto& fx : effects) {
                    if (fx.name == "Black & White") {
                        // Формула человеческого восприятия серого
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

                // Защита от пересвета (ограничиваем от 0 до 255)
                row[x * 4 + 0] = std::clamp(b, 0, 255);
                row[x * 4 + 1] = std::clamp(g, 0, 255);
                row[x * 4 + 2] = std::clamp(r, 0, 255);
            }
        }
    }
};