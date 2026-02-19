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
const int EXTRA_UI_HEIGHT = 250; 

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return -1; 

    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    UIManager ui;
    ui.Init(window, renderer);
    ExportUI exportMenu;        
    bool showExportMenu = false;

    std::vector<TimelineTrack> projectTracks = {
        {"Video 1 (Main)", TRACK_VIDEO},
        {"Video 2 (Overlay)", TRACK_VIDEO},
        {"Audio 1 (Music)", TRACK_AUDIO}
    };

    // TODO: [ВАЖНО] МИКШЕР: Создаем по одному плееру на каждую дорожку!
    std::vector<VideoPlayer*> players;
    for (int i = 0; i < projectTracks.size(); i++) {
        players.push_back(new VideoPlayer());
    }
    std::vector<int> lastActiveClipPerTrack(projectTracks.size(), -1);

    std::vector<VideoClip> projectClips;
    int selectedClipIndex = -1;

    bool isPlaying = false;
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
                
                projectClips.push_back({droppedFile, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0});
                selectedClipIndex = projectClips.size() - 1;
                currentProgress = 0.0f;
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) isPlaying = !isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) { currentProgress += 0.05f; if (currentProgress > 1.0f) currentProgress = 1.0f; }
                if (event.key.keysym.sym == SDLK_LEFT)  { currentProgress -= 0.05f; if (currentProgress < 0.0f) currentProgress = 0.0f; }
            }
        }

        // Вычисляем длину проекта (по самому длинному загруженному видео)
        double maxDurationSec = 1.0;
        for (auto p : players) {
            if (p->isLoaded && p->GetDurationSeconds() > maxDurationSec) maxDurationSec = p->GetDurationSeconds();
        }

        if (isPlaying) {
            currentProgress += (float)(dt / maxDurationSec); 
            if (currentProgress >= 1.0f) { currentProgress = 1.0f; isPlaying = false; }
        }

        bool doSeek = false;
        std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, currentProgress, isPlaying, doSeek, projectClips, selectedClipIndex, showExportMenu, projectTracks);
                                           
        if (!newFile.empty()) {
            projectClips.push_back({newFile, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0});
            selectedClipIndex = projectClips.size() - 1;
            currentProgress = 0.0f; 
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        // TODO: [ВАЖНО] РЕНДЕРИМ ВСЕ ДОРОЖКИ СНИЗУ ВВЕРХ (Layering)
        for (int t = 0; t < projectTracks.size(); ++t) {
            int activeClipIndex = -1;
            for (int i = 0; i < projectClips.size(); ++i) {
                if (projectClips[i].trackIndex == t && currentProgress >= projectClips[i].timelineStart && currentProgress < projectClips[i].timelineEnd) {
                    activeClipIndex = i;
                    break;
                }
            }

            if (activeClipIndex != -1) {
                VideoClip& activeClip = projectClips[activeClipIndex];
                
                // Если плеер пуст или там другой файл - грузим нужный!
                if (players[t]->loadedFilepath != activeClip.filepath) {
                    players[t]->LoadVideo(activeClip.filepath, renderer);
                }

                float ratio = (currentProgress - activeClip.timelineStart) / (activeClip.timelineEnd - activeClip.timelineStart);
                float mediaProgress = activeClip.mediaStart + ratio * (activeClip.mediaEnd - activeClip.mediaStart);

                if (doSeek || activeClipIndex != lastActiveClipPerTrack[t]) {
                    players[t]->Seek(mediaProgress);
                }

                players[t]->isPlaying = isPlaying; 
                players[t]->currentVolume = activeClip.volume;
                
                double targetTimeSec = mediaProgress * players[t]->GetDurationSeconds();
                bool isVideoTrack = (projectTracks[t].type == TRACK_VIDEO);
                
                players[t]->UpdateAndDraw(renderer, WINDOW_VIEW_W - 300, WINDOW_VIEW_H, targetTimeSec, isVideoTrack); 
            } 
            else {
                players[t]->isPlaying = false;
                if (lastActiveClipPerTrack[t] != -1) players[t]->ClearAudio(); 
            }
            lastActiveClipPerTrack[t] = activeClipIndex;
        }

        exportMenu.Draw(&showExportMenu);
        ui.DrawSurface(renderer); 

        SDL_RenderPresent(renderer);
    } 

    ui.Shutdown();
    for(auto p : players) delete p;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}