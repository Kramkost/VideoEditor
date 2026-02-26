#include <iostream>
#include <string>
#include <vector>
#include <cmath> 
#include <algorithm>
#include "export_ui.h"
#include "imgui.h"

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

    PluginManager::InitAndScanPlugins();

    std::vector<TimelineTrack> projectTracks = {
        {"Video 1 (Main)", TRACK_VIDEO},
        {"Video 2 (Overlay)", TRACK_VIDEO},
        {"Audio 1 (Music)", TRACK_AUDIO}
    };

    std::vector<VideoPlayer*> players;
    for (int i = 0; i < projectTracks.size(); i++) {
        players.push_back(new VideoPlayer());
    }
    std::vector<int> lastActiveClipPerTrack(projectTracks.size(), -1);

    std::vector<VideoClip> projectClips;
    std::vector<std::string> projectFiles; // НОВОЕ: Наш Project Bin (Медиапул)
    int selectedClipIndex = -1;

    bool isPlaying = false;
    bool isRunning = true;
    SDL_Event event;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    double perfFrequency = (double)SDL_GetPerformanceFrequency(); 
    float currentProgress = 0.0f; 

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    int lastMouseX = 0, lastMouseY = 0;

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
                
                // ПРОФЕССИОНАЛЬНО: Файл летит не на таймлайн, а в Project Bin!
                if (std::find(projectFiles.begin(), projectFiles.end(), droppedFile) == projectFiles.end()) {
                    projectFiles.push_back(droppedFile);
                }
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_SPACE) isPlaying = !isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) { currentProgress += 0.05f; if (currentProgress > 1.0f) currentProgress = 1.0f; }
                if (event.key.keysym.sym == SDLK_LEFT)  { currentProgress -= 0.05f; if (currentProgress < 0.0f) currentProgress = 0.0f; }
            }
        }

        int leftPanelW = 220;
        int rightPanelW = 300;
        int viewW = WINDOW_VIEW_W - leftPanelW - rightPanelW;
        
        int mouseX, mouseY;
        Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
        bool inPreviewArea = (mouseX > leftPanelW) && (mouseX < WINDOW_VIEW_W - rightPanelW) && (mouseY < WINDOW_VIEW_H);
        
        if (inPreviewArea && (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ImGui::GetIO().WantCaptureMouse) {
            if (selectedClipIndex != -1 && selectedClipIndex < projectClips.size()) {
                float dx = mouseX - lastMouseX;
                float dy = mouseY - lastMouseY;
                
                auto& clip = projectClips[selectedClipIndex];
                float len = clip.timelineEnd - clip.timelineStart;
                float locProg = (len > 0.001f) ? (currentProgress - clip.timelineStart) / len : 0.0f;
                
                if (clip.animX.isAnimated) {
                    float curValX = clip.animX.GetValue(locProg, clip.posX);
                    clip.animX.AddOrUpdateKey(locProg, curValX + dx);
                } else clip.posX += dx;

                if (clip.animY.isAnimated) {
                    float curValY = clip.animY.GetValue(locProg, clip.posY);
                    clip.animY.AddOrUpdateKey(locProg, curValY + dy);
                } else clip.posY += dy;
            }
        }
        lastMouseX = mouseX; lastMouseY = mouseY;

        double maxDurationSec = 1.0;
        for (auto p : players) {
            if (p->isLoaded && p->GetDurationSeconds() > maxDurationSec) maxDurationSec = p->GetDurationSeconds();
        }

        if (isPlaying) {
            currentProgress += (float)(dt / maxDurationSec); 
            if (currentProgress >= 1.0f) { currentProgress = 1.0f; isPlaying = false; }
        }

        bool doSeek = false;
        bool doAddText = false; 
        bool effectChanged = false; 
        
        // Передаем projectFiles в UI
        std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, currentProgress, isPlaying, doSeek, projectClips, selectedClipIndex, showExportMenu, projectTracks, doAddText, effectChanged, projectFiles);
                                           
        if (!newFile.empty()) {
            // Добавляем импортированный файл в Project Bin
            if (std::find(projectFiles.begin(), projectFiles.end(), newFile) == projectFiles.end()) {
                projectFiles.push_back(newFile);
            }
        }

        if (doAddText) {
            float start = currentProgress;
            float end = start + 0.15f; 
            if (end > 1.0f) end = 1.0f;
            if (projectClips.empty()) { start = 0.0f; end = 1.0f; }
            projectClips.push_back(VideoClip("", start, end, 1, true, "YOUR TEXT HERE"));
            selectedClipIndex = projectClips.size() - 1;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
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
                
                if (activeClip.isText) {
                    players[t]->isPlaying = false;
                    if (lastActiveClipPerTrack[t] != -1) players[t]->ClearAudio(); 
                } 
                else {
                    if (players[t]->loadedFilepath != activeClip.filepath) {
                        players[t]->LoadVideo(activeClip.filepath, renderer);
                    }
                    if (effectChanged) players[t]->textureNeedsUpdate = true;

                    float len = activeClip.timelineEnd - activeClip.timelineStart;
                    float localProg = (len > 0.001f) ? (currentProgress - activeClip.timelineStart) / len : 0.0f;
                    
                    float mediaProgress = activeClip.mediaStart + localProg * (activeClip.mediaEnd - activeClip.mediaStart);
                    double targetTimeSec = mediaProgress * players[t]->GetDurationSeconds();

                    if (doSeek) {
                        players[t]->Seek(mediaProgress);
                    } else if (activeClipIndex != lastActiveClipPerTrack[t]) {
                        if (std::abs(targetTimeSec - players[t]->GetCurrentSec()) > 0.1) {
                            players[t]->Seek(mediaProgress);
                        }
                    }

                    players[t]->isPlaying = isPlaying; 
                    players[t]->currentVolume = activeClip.volume;
                    
                    bool isVideoTrack = (projectTracks[t].type == TRACK_VIDEO);

                    float currentPosX = activeClip.animX.GetValue(localProg, activeClip.posX);
                    float currentPosY = activeClip.animY.GetValue(localProg, activeClip.posY);
                    float currentScale = activeClip.animScale.GetValue(localProg, activeClip.scale);
                    float currentRot = activeClip.animRot.GetValue(localProg, activeClip.rotation);

                    players[t]->UpdateAndDraw(renderer, leftPanelW, 0, viewW, WINDOW_VIEW_H, targetTimeSec, isVideoTrack, 
                                              currentPosX, currentPosY, currentScale, currentRot, activeClip.effects); 
                }
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