#include <stdint.h>
#include <algorithm>
#include <cstdint>
#include "plugins.h"


extern "C" {

    // 1. Имя плагина, которое появится в левом меню
     TITAN_EXPORT const char* GetPluginName() {
        return "Blood Red Filter";
    }

    // 2. Сама математика эффекта (обрабатывает весь кадр)
    TITAN_EXPORT void ProcessFrame(uint8_t* pixels, int width, int height, int pitch, float intensity) {
        for (int y = 0; y < height; ++y) {
            uint8_t* row = pixels + y * pitch;
            for (int x = 0; x < width; ++x) {
                // Пиксели идут в формате BGRA
                int b = row[x * 4 + 0];
                int g = row[x * 4 + 1];
                int r = row[x * 4 + 2];

                // Делаем кровавый эффект: гасим синий и зеленый, выкручиваем красный
                float targetR = std::clamp((int)(r * 1.5f), 0, 255);
                float targetG = g * 0.3f;
                float targetB = b * 0.3f;

                // Применяем в зависимости от интенсивности ползунка UI
                row[x * 4 + 0] = b + (targetB - b) * intensity;
                row[x * 4 + 1] = g + (targetG - g) * intensity;
                row[x * 4 + 2] = r + (targetR - r) * intensity;
            }
        }
    }

}
