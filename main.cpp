#include <iostream>
#include <string>
#include <vector>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "ui.h"
#include "video_player.h"

const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
const int EXTRA_UI_HEIGHT = 200; // Сделал чуть повыше для кнопок клипов

int main(int argc, char* argv[]) {
    // ... (Инициализация SDL остается без изменений) ...
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return -1;

    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    VideoPlayer player; 
    UIManager ui;
    ui.Init(window, renderer);

    // НОВОЕ: Данные нашего проекта
    std::vector<VideoClip> projectClips;
    int selectedClipIndex = -1;

    bool isRunning = true;
    SDL_Event event;

    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            ui.ProcessEvent(&event);
            if (event.type == SDL_QUIT) isRunning = false;
            
            // ... (Обработка клавиш остается без изменений) ...
        }

        float currentProgress = player.GetProgress();
        bool doSeek = false;

        // Передаем массив клипов в UI
        std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, 
                                        currentProgress, player.isPlaying, doSeek, 
                                        projectClips, selectedClipIndex);
        
        if (!newFile.empty()) {
            player.LoadVideo(newFile, renderer); 
            // Когда загружаем новое видео, создаем первый базовый клип (от 0 до 100%)
            projectClips.clear();
            projectClips.push_back({newFile, 0.0f, 1.0f, 1.0f});
            selectedClipIndex = 0;
        }
        
        if (doSeek) {
            player.Seek(currentProgress);
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        
        // Немного сдвигаем видео влево, чтобы оно не залезало под правую панель
        // (Отрисовка видео остается как есть, но мы могли бы адаптировать ширину)
        player.UpdateAndDraw(renderer, WINDOW_VIEW_W - 300, WINDOW_VIEW_H); // Уменьшили ширину отрисовки на 300px
        
        ui.DrawSurface(renderer); 

        SDL_RenderPresent(renderer);
    }

    ui.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}