#include <iostream>
#include <string>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "ui.h"
#include "video_player.h"

const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
const int EXTRA_UI_HEIGHT = 150; 

int main(int argc, char* argv[]) {
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

    bool isRunning = true;
    SDL_Event event;

    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            ui.ProcessEvent(&event);
            if (event.type == SDL_QUIT) isRunning = false;
            
            // НОВОЕ: Обработка горячих клавиш
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) {
                    player.isPlaying = !player.isPlaying; // Пробел = Пауза/Плей
                }
                if (event.key.keysym.sym == SDLK_RIGHT) {
                    float p = player.GetProgress() + 0.05f; // Вправо = +5% по времени
                    if (p > 1.0f) p = 1.0f;
                    player.Seek(p);
                }
                if (event.key.keysym.sym == SDLK_LEFT) {
                    float p = player.GetProgress() - 0.05f; // Влево = -5% по времени
                    if (p < 0.0f) p = 0.0f;
                    player.Seek(p);
                }
            }
        }

        // Собираем данные для UI
        float currentProgress = player.GetProgress();
        bool doSeek = false;

        // Рендерим UI
        std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, currentProgress, player.isPlaying, doSeek);
        
        if (!newFile.empty()) {
            player.LoadVideo(newFile, renderer); 
        }
        
        // Если юзер дернул ползунок в UI - перематываем видео!
        if (doSeek) {
            player.Seek(currentProgress);
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        
        player.UpdateAndDraw(renderer, WINDOW_VIEW_W, WINDOW_VIEW_H); 
        ui.DrawSurface(renderer); 

        SDL_RenderPresent(renderer);
    }

    ui.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}