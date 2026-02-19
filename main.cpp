#include <iostream>
#include <string>
#include <vector>
#include "export_ui.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "ui.h"
#include "video_player.h"

const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
// Увеличили высоту, чтобы мультитрек влез на экран
const int EXTRA_UI_HEIGHT = 250; 

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return -1; 

    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    VideoPlayer player; 
    UIManager ui;
    ui.Init(window, renderer);
    ExportUI exportMenu;        
    bool showExportMenu = false;

    // TODO: [ВАЖНО] РЕГИСТРИРУЕМ НАШИ ДОРОЖКИ (КАК В PREMIERE)
    std::vector<TimelineTrack> projectTracks = {
        {"Video 1 (Main)", TRACK_VIDEO},
        {"Video 2 (Overlay)", TRACK_VIDEO},
        {"Audio 1 (Music/SFX)", TRACK_AUDIO}
    };

    std::vector<VideoClip> projectClips;
    int selectedClipIndex = -1;

    bool isRunning = true;
    SDL_Event event;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    double perfFrequency = (double)SDL_GetPerformanceFrequency(); 
    float currentProgress = 0.0f; 

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    while (isRunning) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float dt = (float)((currentTime - lastTime) / perfFrequency); 
        lastTime = currentTime;

        while (SDL_PollEvent(&event)) {
            ui.ProcessEvent(&event);
            if (event.type == SDL_QUIT) isRunning = false;
            
            if (event.type == SDL_DROPFILE) {
                std::string droppedFile = event.drop.file;
                SDL_free(event.drop.file); 
                
                player.LoadVideo(droppedFile, renderer); 
                projectClips.clear();
                // При загрузке файла кладем его на дорожку 0 (Video 1)
                projectClips.push_back({droppedFile, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0});
                selectedClipIndex = 0;
                currentProgress = 0.0f;
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) player.isPlaying = !player.isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) { currentProgress += 0.05f; if (currentProgress > 1.0f) currentProgress = 1.0f; player.Seek(currentProgress); }
                if (event.key.keysym.sym == SDLK_LEFT)  { currentProgress -= 0.05f; if (currentProgress < 0.0f) currentProgress = 0.0f; player.Seek(currentProgress); }
            }
        }

        if (player.isPlaying) {
            double dur = player.GetDurationSeconds();
            if (dur > 0) {
                currentProgress += (float)(dt / dur); 
                if (currentProgress >= 1.0f) { currentProgress = 1.0f; player.isPlaying = false; }
            }
        }

        bool doSeek = false;
        // Передаем projectTracks в интерфейс!
        std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, 
                                        currentProgress, player.isPlaying, doSeek, 
                                        projectClips, selectedClipIndex, showExportMenu, projectTracks);
                                           
        if (!newFile.empty()) {
            player.LoadVideo(newFile, renderer); 
            projectClips.clear();
            projectClips.push_back({newFile, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0});
            selectedClipIndex = 0;
            currentProgress = 0.0f; 
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        
        // TODO: [ВАЖНО] ИЩЕМ АКТИВНЫЙ КЛИП СВЕРХУ ВНИЗ ПО ДОРОЖКАМ
        // Если клипы накладываются друг на друга, мы покажем тот, что лежит на самой "верхней" видео-дорожке
        int activeClipIndex = -1;
        for (int t = 0; t < projectTracks.size(); ++t) {
            for (int i = 0; i < projectClips.size(); ++i) {
                if (projectClips[i].trackIndex == t && 
                    currentProgress >= projectClips[i].timelineStart && 
                    currentProgress < projectClips[i].timelineEnd) {
                    activeClipIndex = i;
                    break;
                }
            }
            if (activeClipIndex != -1) break; // Нашли самый приоритетный клип, прекращаем поиск!
        }

        static int lastActiveClipIndex = -1;

        if (activeClipIndex != -1) {
            VideoClip& activeClip = projectClips[activeClipIndex];
            
            float ratio = (currentProgress - activeClip.timelineStart) / (activeClip.timelineEnd - activeClip.timelineStart);
            float mediaProgress = activeClip.mediaStart + ratio * (activeClip.mediaEnd - activeClip.mediaStart);

            if (doSeek || activeClipIndex != lastActiveClipIndex) {
                player.Seek(mediaProgress);
            }

            double targetTimeSec = mediaProgress * player.GetDurationSeconds();
            player.currentVolume = activeClip.volume;
            player.UpdateAndDraw(renderer, WINDOW_VIEW_W - 300, WINDOW_VIEW_H, targetTimeSec); 
        } 
        else {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect blackScreen = {0, 0, WINDOW_VIEW_W - 300, WINDOW_VIEW_H};
            SDL_RenderFillRect(renderer, &blackScreen);
            
            if (lastActiveClipIndex != -1) {
                player.ClearAudio(); 
            }
        }
        
        lastActiveClipIndex = activeClipIndex;

        exportMenu.Draw(&showExportMenu);
        ui.DrawSurface(renderer); 

        SDL_RenderPresent(renderer);
    } 

    ui.Shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}