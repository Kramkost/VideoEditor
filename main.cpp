/* =========================================================================
 * TITAN VIDEO EDITOR - MAIN APPLICATION LOOP
 * =========================================================================
 * RECENT UPDATES:
 * - Real FFmpeg CLI Export Pipeline (Captures C++ Render Target)
 * - Dynamic Track Management (+ Add Track sync)
 * - Start Screen / Project Management integration
 * ========================================================================= */

#include <iostream>
#include <string>
#include <vector>
#include <cmath> 
#include <algorithm>

// --- CORE SYSTEM INCLUDES ---
#include "export_ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"         
#include "imgui_impl_sdlrenderer2.h" 
#include "project_manager.h"         

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include "ui.h"
#include "video_player.h"

const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
const int EXTRA_UI_HEIGHT = 250; 

int main(int argc, char* argv[]) {
    // 1. INITIALIZATION: SDL, Window, and Renderer
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) return -1; 

    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    // 2. INITIALIZATION: UI & Modules
    UIManager ui;
    ui.Init(window, renderer);
    ExportUI exportMenu;        
    bool showExportMenu = false;

    PluginManager::InitAndScanPlugins();
    ProjectManager::Init(); 

    // 3. APPLICATION STATE VARIABLES
    ProjectData currentProject;  
    bool isProjectOpen = false;  
    
    std::vector<VideoPlayer*> players;
    std::vector<int> lastActiveClipPerTrack;
    int selectedClipIndex = -1;

    bool isPlaying = false;
    bool isRunning = true;
    SDL_Event event;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    double perfFrequency = (double)SDL_GetPerformanceFrequency(); 
    float currentProgress = 0.0f; 

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    int lastMouseX = 0, lastMouseY = 0;

    // 4. EXPORT VARIABLES (НОВЫЕ ПЕРЕМЕННЫЕ)
    bool isExporting = false;
    int exportFrameCurrent = 0;
    int exportFrameTotal = 0;
    FILE* ffmpegPipe = nullptr;
    std::vector<uint8_t> exportPixelBuffer;

    // =========================================================================
    // MAIN APPLICATION LOOP
    // =========================================================================
    while (isRunning) {
        // Calculate Delta Time (dt)
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float dt = (float)((currentTime - lastTime) / perfFrequency); 
        lastTime = currentTime;

        // --- EVENT HANDLING ---
        while (SDL_PollEvent(&event)) {
            ui.ProcessEvent(&event); // Always send events to UI first
            if (event.type == SDL_QUIT) isRunning = false;
            
            if (event.type == SDL_DROPFILE) {
                std::string droppedFile = event.drop.file;
                SDL_free(event.drop.file); 
                
                if (isProjectOpen) {
                    if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), droppedFile) == currentProject.mediaFiles.end()) {
                        currentProject.mediaFiles.push_back(droppedFile);
                    }
                }
            }
            
            if (event.type == SDL_KEYDOWN && isProjectOpen && !isExporting) {
                if (event.key.keysym.sym == SDLK_SPACE) isPlaying = !isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) { currentProgress += 0.05f; if (currentProgress > 1.0f) currentProgress = 1.0f; }
                if (event.key.keysym.sym == SDLK_LEFT)  { currentProgress -= 0.05f; if (currentProgress < 0.0f) currentProgress = 0.0f; }
                
                if (event.key.keysym.sym == SDLK_s && (SDL_GetModState() & KMOD_CTRL)) {
                    if (currentProject.saveFilepath.empty()) ProjectManager::SaveProjectAs(currentProject);
                    else ProjectManager::SaveProject(currentProject);
                }
            }
        }

        // =========================================================================
        // STATE 1: START SCREEN (No project loaded)
        // =========================================================================
        if (!isProjectOpen) {
            SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255); // Dark background
            SDL_RenderClear(renderer);

            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (ProjectManager::DrawStartScreen(WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, currentProject)) {
                isProjectOpen = true; 
                
                for (auto p : players) delete p; 
                players.clear(); lastActiveClipPerTrack.clear();
                
                for (int i = 0; i < currentProject.tracks.size(); i++) {
                    players.push_back(new VideoPlayer());
                    lastActiveClipPerTrack.push_back(-1);
                }
            }

            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        } 
        
        // =========================================================================
        // STATE 2: MAIN EDITOR (Project is loaded)
        // =========================================================================
        else {
            // --- СИНХРОНИЗАЦИЯ НОВЫХ ДОРОЖЕК ИЗ UI ---
            while (players.size() < currentProject.tracks.size()) {
                players.push_back(new VideoPlayer());
                lastActiveClipPerTrack.push_back(-1);
            }

            int leftPanelW = 220;
            int rightPanelW = 300;
            int viewW = WINDOW_VIEW_W - leftPanelW - rightPanelW;
            
            // --- УПРАВЛЕНИЕ ЭКСПОРТОМ (MAGIC HAPPENS HERE) ---
            double maxDurationSec = 1.0;
            for (auto p : players) {
                if (p->isLoaded && p->GetDurationSeconds() > maxDurationSec) maxDurationSec = p->GetDurationSeconds();
            }
            
            if (exportMenu.startRender) {
                exportMenu.startRender = false;
                isExporting = true;
                isPlaying = false;
                exportFrameCurrent = 0;
                
                float maxTimelineEnd = 0.0f;
                for (const auto& clip : currentProject.clips) {
                    if (clip.timelineEnd > maxTimelineEnd) maxTimelineEnd = clip.timelineEnd;
                }
                exportFrameTotal = (int)(maxTimelineEnd * maxDurationSec * exportMenu.fps);
                if (exportFrameTotal == 0) isExporting = false; 
                
                if (isExporting) {
                    exportPixelBuffer.resize(viewW * WINDOW_VIEW_H * 4); 
                    
                    std::string cmd = "ffmpeg -y -f rawvideo -pix_fmt bgra -s " + std::to_string(viewW) + "x" + std::to_string(WINDOW_VIEW_H) + 
                                      " -r " + std::to_string(exportMenu.fps) + " -i - -vf scale=" + std::to_string(exportMenu.width) + ":" + std::to_string(exportMenu.height) + 
                                      " -c:v libx264 -preset fast -crf 23 -pix_fmt yuv420p \"" + exportMenu.outputPath + "\"";
                    
                    #ifdef _WIN32
                    ffmpegPipe = _popen(cmd.c_str(), "wb");
                    #else
                    ffmpegPipe = popen(cmd.c_str(), "w");
                    #endif
                    
                    if (!ffmpegPipe) {
                        std::cerr << "FAILED TO START FFMPEG EXPORT!\n";
                        isExporting = false;
                    }
                }
            }

            // --- ОБЫЧНАЯ ЛОГИКА (Если не экспортируем) ---
            bool doSeek = false;
            bool doAddText = false; 
            bool effectChanged = false; 
            
            if (!isExporting) {
                int mouseX, mouseY;
                Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
                bool inPreviewArea = (mouseX > leftPanelW) && (mouseX < WINDOW_VIEW_W - rightPanelW) && (mouseY < WINDOW_VIEW_H);
                
                if (inPreviewArea && (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ImGui::GetIO().WantCaptureMouse) {
                    if (selectedClipIndex != -1 && selectedClipIndex < currentProject.clips.size()) {
                        float dx = mouseX - lastMouseX;
                        float dy = mouseY - lastMouseY;
                        
                        auto& clip = currentProject.clips[selectedClipIndex];
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

                if (isPlaying) {
                    currentProgress += (float)(dt / maxDurationSec); 
                    if (currentProgress >= 1.0f) { currentProgress = 1.0f; isPlaying = false; }
                }
            } else {
                // Если мы ЭКСПОРТИРУЕМ, жестко двигаем ползунок покадрово
                currentProgress = (float)exportFrameCurrent / exportFrameTotal;
                doSeek = true; 
            }

            // --- UI RENDERING ---
            // --- UI RENDERING ---
            std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, 
                                            currentProgress, isPlaying, doSeek, 
                                            currentProject.clips, selectedClipIndex, 
                                            showExportMenu, currentProject.tracks, 
                                            doAddText, effectChanged, currentProject.mediaFiles);
                                            
            // --- ОБРАБОТКА КНОПОК СОХРАНЕНИЯ ИЗ UI ---
            if (ui.triggerSave) {
                ui.triggerSave = false;
                if (currentProject.saveFilepath.empty()) ProjectManager::SaveProjectAs(currentProject);
                else ProjectManager::SaveProject(currentProject);
            }
            if (ui.triggerSaveAs) {
                ui.triggerSaveAs = false;
                ProjectManager::SaveProjectAs(currentProject);
            }
                                               
            if (!newFile.empty()) {
                if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), newFile) == currentProject.mediaFiles.end()) {
                }
            }

            if (doAddText) {
                float start = currentProgress;
                float end = start + 0.15f; 
                if (end > 1.0f) end = 1.0f;
                if (currentProject.clips.empty()) { start = 0.0f; end = 1.0f; }
                currentProject.clips.push_back(VideoClip("", start, end, 1, true, "YOUR TEXT HERE"));
                selectedClipIndex = currentProject.clips.size() - 1;
            }

            // --- VIDEO RENDERING ---
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            for (int t = 0; t < currentProject.tracks.size(); ++t) {
                int activeClipIndex = -1;
                for (int i = 0; i < currentProject.clips.size(); ++i) {
                    if (currentProject.clips[i].trackIndex == t && currentProgress >= currentProject.clips[i].timelineStart && currentProgress < currentProject.clips[i].timelineEnd) {
                        activeClipIndex = i;
                        break;
                    }
                }

                if (activeClipIndex != -1) {
                    VideoClip& activeClip = currentProject.clips[activeClipIndex];
                    
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

                        // Отключаем звук во время экспорта
                        players[t]->isPlaying = isExporting ? false : isPlaying; 
                        players[t]->currentVolume = activeClip.volume;
                        
                        bool isVideoTrack = (currentProject.tracks[t].type == TRACK_VIDEO);

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

            // --- ЗАХВАТ КАДРА ДЛЯ ЭКСПОРТА ---
            if (isExporting && ffmpegPipe) {
                // 1. Читаем пиксели только из зоны превью видео
                SDL_Rect exportRect = { leftPanelW, 0, viewW, WINDOW_VIEW_H };
                SDL_RenderReadPixels(renderer, &exportRect, SDL_PIXELFORMAT_ARGB8888, exportPixelBuffer.data(), viewW * 4);
                
                // 2. Отправляем в FFmpeg
                fwrite(exportPixelBuffer.data(), 1, exportPixelBuffer.size(), ffmpegPipe);
                
                exportFrameCurrent++;
                
                // 3. Рисуем UI ПОВЕРХ, чтобы пользователь видел прогресс
                ui.DrawSurface(renderer);
                
                // Прогресс-бар загрузки
                SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255);
                SDL_Rect progressRect = { 0, WINDOW_VIEW_H + EXTRA_UI_HEIGHT - 10, (int)((float)exportFrameCurrent / exportFrameTotal * WINDOW_VIEW_W), 10 };
                SDL_RenderFillRect(renderer, &progressRect);
                
                if (exportFrameCurrent >= exportFrameTotal) {
                    isExporting = false;
                    #ifdef _WIN32
                    _pclose(ffmpegPipe);
                    #else
                    pclose(ffmpegPipe);
                    #endif
                    ffmpegPipe = nullptr;
                    currentProgress = 0.0f; // Возвращаем в начало
                }
            } else {
                exportMenu.Draw(&showExportMenu);
                ui.DrawSurface(renderer); 
            }
            
            SDL_RenderPresent(renderer);
        }
    } 

    // --- CLEANUP ---
    ui.Shutdown();
    for(auto p : players) delete p;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}