/* =========================================================================
 * TITAN VIDEO EDITOR - MAIN APPLICATION LOOP
 * =========================================================================
 * This file contains the main entry point and core loop of the application.
 * It handles SDL initialization, input events, and manages the application
 * state switching between the "Start Screen" and the "Main Editor".
 *
 * RECENT UPDATES:
 * - Integrated 'ProjectManager' for creating, loading, and saving projects.
 * - Replaced scattered vectors with a unified 'ProjectData' structure.
 * - Added a dedicated Start Screen that appears before the editor loads.
 * - Project Media Bin (Media files) is now saved into the project state.
 * ========================================================================= */

#include <iostream>
#include <string>
#include <vector>
#include <cmath> 
#include <algorithm>

// --- CORE SYSTEM INCLUDES ---
#include "export_ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"         // Added for Start Screen ImGui handling
#include "imgui_impl_sdlrenderer2.h" // Added for Start Screen ImGui handling
#include "project_manager.h"         // NEW: Project System integration

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
    ProjectManager::Init(); // NEW: Initialize project system (loads recent list)

    // 3. APPLICATION STATE VARIABLES
    ProjectData currentProject;  // NEW: Holds all tracks, clips, and media files
    bool isProjectOpen = false;  // NEW: Toggle between Start Screen and Editor
    
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
            
            // Handle Drag & Drop ONLY if a project is open
            if (event.type == SDL_DROPFILE) {
                std::string droppedFile = event.drop.file;
                SDL_free(event.drop.file); 
                
                if (isProjectOpen) {
                    if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), droppedFile) == currentProject.mediaFiles.end()) {
                        currentProject.mediaFiles.push_back(droppedFile);
                    }
                }
            }
            
            // Handle Keyboard inputs ONLY if a project is open
            if (event.type == SDL_KEYDOWN && isProjectOpen) {
                if (event.key.keysym.sym == SDLK_SPACE) isPlaying = !isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) { currentProgress += 0.05f; if (currentProgress > 1.0f) currentProgress = 1.0f; }
                if (event.key.keysym.sym == SDLK_LEFT)  { currentProgress -= 0.05f; if (currentProgress < 0.0f) currentProgress = 0.0f; }
                
                // NEW: Quick Save shortcut (Ctrl+S)
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

            // Manual ImGui frame setup for Start Screen
            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            // Render Start Screen. Returns true if user created or loaded a project.
            if (ProjectManager::DrawStartScreen(WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, currentProject)) {
                isProjectOpen = true; 
                
                // Initialize VideoPlayers based on the loaded project tracks
                for (auto p : players) delete p; // Clear old players if any
                players.clear();
                
                for (int i = 0; i < currentProject.tracks.size(); i++) {
                    players.push_back(new VideoPlayer());
                }
                lastActiveClipPerTrack.assign(currentProject.tracks.size(), -1);
            }

            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        } 
        
        // =========================================================================
        // STATE 2: MAIN EDITOR (Project is loaded)
        // =========================================================================
        else {
            int leftPanelW = 220;
            int rightPanelW = 300;
            int viewW = WINDOW_VIEW_W - leftPanelW - rightPanelW;
            
            // --- MOUSE PREVIEW INTERACTION ---
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

            // --- TIMELINE PLAYBACK PROGRESS ---
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
            
            // --- UI RENDERING ---
            // Pass all project data instead of loose vectors
            std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, 
                                            currentProgress, isPlaying, doSeek, 
                                            currentProject.clips, selectedClipIndex, 
                                            showExportMenu, currentProject.tracks, 
                                            doAddText, effectChanged, currentProject.mediaFiles);
                                               
            // Handle returned file from Import button
            if (!newFile.empty()) {
                if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), newFile) == currentProject.mediaFiles.end()) {
                    currentProject.mediaFiles.push_back(newFile);
                }
            }

            // Handle add text button
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
                // Find which clip is currently active on this track
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
                        // Load video if it changed
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
                        
                        bool isVideoTrack = (currentProject.tracks[t].type == TRACK_VIDEO);

                        float currentPosX = activeClip.animX.GetValue(localProg, activeClip.posX);
                        float currentPosY = activeClip.animY.GetValue(localProg, activeClip.posY);
                        float currentScale = activeClip.animScale.GetValue(localProg, activeClip.scale);
                        float currentRot = activeClip.animRot.GetValue(localProg, activeClip.rotation);

                        // Draw video frame to screen
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
            ui.DrawSurface(renderer); // This handles the ImGui rendering for the editor
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