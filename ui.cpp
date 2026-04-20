#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h> 
#ifdef _WIN32
    #include <windows.h>
#endif
#include <string>
#include "logger.h"
#include <fstream>

void UIManager::Init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    LOG_DEBUG("UIManager Initialized successfully.");
}

void UIManager::ProcessEvent(const SDL_Event* event) { ImGui_ImplSDL2_ProcessEvent(event); }

void UIManager::SaveEffectPreset(const std::string& name, const std::vector<EffectParams>& effects) {
    if (effects.empty()) return;
    std::string path = "plugins/presets_" + name + ".txt";
    std::ofstream out(path);
    if (!out) { LOG_ERROR("Failed to write preset to %s", path.c_str()); return; }
    
    for (const auto& fx : effects) out << fx.name << "=" << fx.intensity << "\n";
    LOG_DEBUG("Saved %zu effects to preset %s", effects.size(), path.c_str());
}

void UIManager::LoadEffectPreset(const std::string& name, std::vector<EffectParams>& outEffects) {
    std::string path = "plugins/presets_" + name + ".txt";
    std::ifstream in(path);
    if (!in) { LOG_ERROR("Failed to load preset %s", path.c_str()); return; }
    
    outEffects.clear();
    std::string line;
    while (std::getline(in, line)) {
        size_t delim = line.find('=');
        if (delim != std::string::npos) {
            outEffects.push_back({line.substr(0, delim), std::stof(line.substr(delim + 1))});
        }
    }
    LOG_DEBUG("Loaded preset %s", path.c_str());
}

std::string UIManager::OpenFileDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn; CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Video Files\0*.mp4;*.mkv;*.avi;*.mov;*.jpg;*.png\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
#endif
    return "";
}

std::string UIManager::Render(int windowW, int windowH, int uiHeight, 
                              float& progress, bool& isPlaying, bool& doSeek,
                              std::vector<VideoClip>& clips, int& selectedClipIndex,
                              bool& showExport, std::vector<TimelineTrack>& tracks,
                              bool& doAddText, bool& effectChanged,
                              std::vector<std::string>& projectFiles) {
    std::string selectedFile = "";
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    float menuHeight = 0.0f;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save Project", "Ctrl+S")) triggerSave = true;
            if (ImGui::MenuItem("Save As...")) triggerSaveAs = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Settings")) showSettings = true;
            if (ImGui::MenuItem("Export")) showExport = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Workspace")) {
            if (ImGui::MenuItem("Editing", "F5", currentWorkspace == WORKSPACE_EDITING)) { currentWorkspace = WORKSPACE_EDITING; LOG_DEBUG("Switched to EDITING Workspace"); }
            if (ImGui::MenuItem("Effects", "F6", currentWorkspace == WORKSPACE_EFFECTS)) { currentWorkspace = WORKSPACE_EFFECTS; LOG_DEBUG("Switched to EFFECTS Workspace"); }
            ImGui::EndMenu();
        }
        menuHeight = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }

    if (showSettings) {
        ImGui::SetNextWindowPos(ImVec2(windowW / 2 - 200, windowH / 2 - 150), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Preferences", &showSettings, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "General Settings");
            ImGui::Separator(); ImGui::Spacing();
            static bool darkTheme = true;
            if (ImGui::Checkbox("Dark Theme", &darkTheme)) {
                if (darkTheme) ImGui::StyleColorsDark(); else ImGui::StyleColorsLight();
            }
            static float uiScale = 1.0f;
            ImGui::SliderFloat("UI Scale", &uiScale, 0.8f, 1.5f, "%.1f");
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Performance");
            ImGui::Separator(); ImGui::Spacing();
            static bool proxyMode = false;
            ImGui::Checkbox("Enable Proxy Mode (1/4 Res)", &proxyMode);
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            if (ImGui::Button("Close & Save", ImVec2(-1, 30))) showSettings = false;
        }
        ImGui::End();
    }

    int leftPanelWidth = (currentWorkspace == WORKSPACE_EFFECTS) ? 350 : 220;
    int rightPanelWidth = (currentWorkspace == WORKSPACE_EFFECTS) ? 0 : 300;

    ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < clips.size(); i++) {
        if (clips[i].isText && progress >= clips[i].timelineStart && progress < clips[i].timelineEnd) {
            float len = clips[i].timelineEnd - clips[i].timelineStart;
            float localTime = (len > 0.001f) ? (progress - clips[i].timelineStart) / len : 0.0f;
            float curX = clips[i].animX.GetValue(localTime, clips[i].posX);
            float curY = clips[i].animY.GetValue(localTime, clips[i].posY);
            float curScale = clips[i].animScale.GetValue(localTime, clips[i].scale);
            float fontSize = 64.0f * curScale; 
            ImVec2 textSize = ImGui::CalcTextSize(clips[i].textContent.c_str());
            float screenX = leftPanelWidth + (windowW - leftPanelWidth - rightPanelWidth) / 2.0f + curX - (textSize.x * curScale) / 2.0f;
            float screenY = windowH / 2.0f + curY - (textSize.y * curScale) / 2.0f;
            bg_draw_list->AddText(ImGui::GetFont(), fontSize, ImVec2(screenX + 2, screenY + 2), IM_COL32(0,0,0,255), clips[i].textContent.c_str());
            bg_draw_list->AddText(ImGui::GetFont(), fontSize, ImVec2(screenX, screenY), IM_COL32(255,255,255,255), clips[i].textContent.c_str());
        }
    }

    if (currentWorkspace == WORKSPACE_EDITING) {
        ImGui::SetNextWindowPos(ImVec2(0, menuHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH / 2 - menuHeight), ImGuiCond_Always);
        ImGui::Begin("Effects Library", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Video Plugins:");
        ImGui::Separator(); ImGui::Spacing();
        auto availablePlugins = PluginManager::GetAvailablePlugins();
        for (const auto& plugin : availablePlugins) {
            if (ImGui::Button(plugin.c_str(), ImVec2(-1, 35))) {
                if (selectedClipIndex >= 0 && selectedClipIndex < clips.size() && !clips[selectedClipIndex].isText) {
                    clips[selectedClipIndex].effects.push_back({plugin, 1.0f}); effectChanged = true;
                }
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0, windowH / 2), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - uiHeight - windowH / 2), ImGuiCond_Always);
        ImGui::Begin("Project Bin", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Media Files:");
        ImGui::Separator();
        for (int i = 0; i < projectFiles.size(); ++i) {
            std::string shortName = projectFiles[i].substr(projectFiles[i].find_last_of("/\\") + 1);
            ImGui::Selectable(shortName.c_str());
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                ImGui::SetDragDropPayload("PROJECT_FILE", &i, sizeof(int));
                ImGui::Text("Dragging: %s", shortName.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, menuHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - uiHeight - menuHeight), ImGuiCond_Always);
        ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        ImGui::Text("Properties"); ImGui::Separator();
        if (selectedClipIndex >= 0 && selectedClipIndex < clips.size()) {
            ImGui::Spacing();
            if (clips[selectedClipIndex].isText) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Text Content");
                char textBuf[256]; strncpy(textBuf, clips[selectedClipIndex].textContent.c_str(), sizeof(textBuf));
                if (ImGui::InputTextMultiline("##text", textBuf, sizeof(textBuf), ImVec2(-1, 50))) clips[selectedClipIndex].textContent = std::string(textBuf);
                ImGui::Spacing();
            }
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Transform & Animation");
            float len = clips[selectedClipIndex].timelineEnd - clips[selectedClipIndex].timelineStart;
            float localTime = (len > 0.001f) ? (progress - clips[selectedClipIndex].timelineStart) / len : 0.0f;
            localTime = std::clamp(localTime, 0.0f, 1.0f);

            auto DrawAnimParam = [&](const char* label, AnimTrack& track, float& baseVal, float speed, float minV, float maxV) {
                ImGui::PushID(label);
                ImVec4 btnColor = track.isAnimated ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                if (ImGui::Button(track.isAnimated ? " O " : " - ")) {
                    track.isAnimated = !track.isAnimated;
                    if (track.isAnimated) track.AddOrUpdateKey(localTime, baseVal); else track.keys.clear();
                }
                ImGui::PopStyleColor(); ImGui::SameLine();
                
                float val = track.GetValue(localTime, baseVal);
                if (ImGui::DragFloat(label, &val, speed, minV, maxV)) {
                    if (track.isAnimated) track.AddOrUpdateKey(localTime, val); else baseVal = val;
                }
                ImGui::PopID();
            };

            DrawAnimParam("Pos X", clips[selectedClipIndex].animX, clips[selectedClipIndex].posX, 1.0f, -3000, 3000);
            DrawAnimParam("Pos Y", clips[selectedClipIndex].animY, clips[selectedClipIndex].posY, 1.0f, -3000, 3000);
            DrawAnimParam("Scale", clips[selectedClipIndex].animScale, clips[selectedClipIndex].scale, 0.01f, 0.01f, 10.0f);
            if (!clips[selectedClipIndex].isText) DrawAnimParam("Rotation", clips[selectedClipIndex].animRot, clips[selectedClipIndex].rotation, 1.0f, -360.0f, 360.0f);
            
            ImGui::Spacing();
            if (!clips[selectedClipIndex].isText) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Audio");
                ImGui::SliderFloat("Volume", &clips[selectedClipIndex].volume, 0.0f, 1.0f, "%.2f");
            }
        } else ImGui::TextDisabled("Select a clip to edit properties.");
        ImGui::End();

    } else if (currentWorkspace == WORKSPACE_EFFECTS) {
        ImGui::SetNextWindowPos(ImVec2(0, menuHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - uiHeight - menuHeight), ImGuiCond_Always);
        ImGui::Begin("Effect Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        
        if (selectedClipIndex >= 0 && selectedClipIndex < clips.size() && !clips[selectedClipIndex].isText) {
            static char presetName[64] = "MyPreset";
            ImGui::InputText("##presetName", presetName, sizeof(presetName));
            ImGui::SameLine();
            if (ImGui::Button("Save Preset")) SaveEffectPreset(presetName, clips[selectedClipIndex].effects);
            if (ImGui::Button("Load Preset")) { LoadEffectPreset(presetName, clips[selectedClipIndex].effects); effectChanged = true; }
            ImGui::Separator();
            
            if (ImGui::BeginCombo("Add Effect", "Select...")) {
                for (const auto& plugin : PluginManager::GetAvailablePlugins()) {
                    if (ImGui::Selectable(plugin.c_str())) {
                        clips[selectedClipIndex].effects.push_back({plugin, 1.0f}); effectChanged = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Separator();
            for (int e = 0; e < clips[selectedClipIndex].effects.size(); ++e) {
                auto& fx = clips[selectedClipIndex].effects[e];
                ImGui::PushID(e); ImGui::Text("%s", fx.name.c_str()); ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                if (ImGui::Button("X")) { clips[selectedClipIndex].effects.erase(clips[selectedClipIndex].effects.begin() + e); effectChanged = true; ImGui::PopID(); break; }
                if (ImGui::SliderFloat("Intensity", &fx.intensity, 0.0f, 1.0f)) effectChanged = true;
                ImGui::PopID(); ImGui::Spacing();
            }
        } else {
            ImGui::TextDisabled("Select a video clip to edit effects.");
        }
        ImGui::End();
    }

    // === BOTTOM TIMELINE ===
    ImGui::SetNextWindowPos(ImVec2(0, windowH - uiHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, uiHeight), ImGuiCond_Always);
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    
    if (ImGui::Button("Import File", ImVec2(100, 30))) { selectedFile = OpenFileDialog(); }
    ImGui::SameLine(); 
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f)); 
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f)); 
    if (ImGui::Button("+ ADD TEXT", ImVec2(100, 30))) { doAddText = true; } 
    ImGui::PopStyleColor(2);
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
    if (ImGui::Button(isPlaying ? "PAUSE" : "PLAY", ImVec2(80, 30))) isPlaying = !isPlaying;
    ImGui::PopStyleColor();
    ImGui::SameLine(); 
    if (ImGui::Button("CUT (Split)", ImVec2(100, 30))) {
        if (clips.size() > 0 && selectedClipIndex >= 0) {
            VideoClip newClip = clips[selectedClipIndex];
            float ratio = (progress - clips[selectedClipIndex].timelineStart) / (clips[selectedClipIndex].timelineEnd - clips[selectedClipIndex].timelineStart);
            float splitMedia = clips[selectedClipIndex].mediaStart + ratio * (clips[selectedClipIndex].mediaEnd - clips[selectedClipIndex].mediaStart);
            newClip.timelineStart = progress; newClip.mediaStart = splitMedia;
            clips[selectedClipIndex].timelineEnd = progress; clips[selectedClipIndex].mediaEnd = splitMedia;
            clips.insert(clips.begin() + selectedClipIndex + 1, newClip);
            selectedClipIndex++; 
        }
    } 
    ImGui::SameLine(); if (clips.empty()) ImGui::BeginDisabled(); 
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f)); 
    if (ImGui::Button("EXPORT VIDEO", ImVec2(120, 30))) showExport = true; 
    ImGui::PopStyleColor(); if (clips.empty()) ImGui::EndDisabled(); 

    ImGui::Spacing(); ImGui::Separator();
    
    ImGui::BeginChild("TracksScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float headerW = 140.0f;
    float trackWidth = ImGui::GetContentRegionAvail().x - headerW;
    float trackHeight = 40.0f; 
    float trackSpacing = 4.0f;
    float totalTimelineHeight = (tracks.size() + 1) * (trackHeight + trackSpacing);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (int t = 0; t < tracks.size(); ++t) {
        float currentY = p.y + t * (trackHeight + trackSpacing);
        draw_list->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + headerW - 4, currentY + trackHeight), IM_COL32(30, 30, 30, 255), 4.0f);
        draw_list->AddText(ImVec2(p.x + 10, currentY + 12), IM_COL32(200, 200, 200, 255), tracks[t].name.c_str());
        ImU32 bgColor = (tracks[t].type == TRACK_VIDEO) ? IM_COL32(40, 40, 45, 255) : IM_COL32(35, 45, 40, 255);
        draw_list->AddRectFilled(ImVec2(p.x + headerW, currentY), ImVec2(p.x + headerW + trackWidth, currentY + trackHeight), bgColor);
    }

    float plusY = p.y + tracks.size() * (trackHeight + trackSpacing) + 5.0f; 
    ImGui::SetCursorScreenPos(ImVec2(p.x + 10, plusY)); 
    float time = ImGui::GetTime();
    float pulse = (std::sin(time * 5.0f) + 1.0f) * 0.5f; 
    ImVec4 addBtnColor = ImVec4(0.2f + 0.1f * pulse, 0.4f + 0.1f * pulse, 0.6f + 0.2f * pulse, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, addBtnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
    
    if (ImGui::Button("+ ADD TRACK", ImVec2(headerW - 24, 25))) ImGui::OpenPopup("AddTrackMenu");
    ImGui::PopStyleColor(2);

    if (ImGui::BeginPopup("AddTrackMenu")) {
        if (ImGui::MenuItem("Add Video Track")) tracks.push_back({"Video " + std::to_string(tracks.size() + 1), TRACK_VIDEO});
        if (ImGui::MenuItem("Add Audio Track")) tracks.push_back({"Audio " + std::to_string(tracks.size() + 1), TRACK_AUDIO});
        ImGui::EndPopup();
    }

    for (int i = 0; i < clips.size(); i++) {
        int t = clips[i].trackIndex; if (t < 0 || t >= tracks.size()) continue; 

        float currentY = p.y + t * (trackHeight + trackSpacing);
        float x1 = p.x + headerW + (clips[i].timelineStart * trackWidth);
        float x2 = p.x + headerW + (clips[i].timelineEnd * trackWidth);
        
        float targetScale = clips[i].isInteracting ? 1.1f : 1.0f;
        clips[i].visualScale += (targetScale - clips[i].visualScale) * 0.3f; 

        float clipW = x2 - x1;
        float centerX = x1 + clipW * 0.5f;
        float centerY = currentY + trackHeight * 0.5f;

        float dX1 = centerX - (clipW * 0.5f) * clips[i].visualScale;
        float dX2 = centerX + (clipW * 0.5f) * clips[i].visualScale;
        float dY1 = centerY - (trackHeight * 0.5f) * clips[i].visualScale;
        float dY2 = centerY + (trackHeight * 0.5f) * clips[i].visualScale;

        ImU32 clipColor = (i == selectedClipIndex) ? IM_COL32(100, 150, 220, 255) : IM_COL32(50, 100, 160, 255);
        if (tracks[t].type == TRACK_AUDIO && i != selectedClipIndex) clipColor = IM_COL32(50, 160, 100, 255); 
        if (clips[i].isText && i != selectedClipIndex) clipColor = IM_COL32(160, 100, 50, 255); 
        
        draw_list->AddRectFilled(ImVec2(dX1 + 1, dY1 + 2), ImVec2(dX2 - 1, dY2 - 2), clipColor, 4.0f);
        
        char label[64]; 
        if (clips[i].isText) sprintf(label, "Text: %s", clips[i].textContent.c_str());
        else {
            std::string shortN = clips[i].filepath.substr(clips[i].filepath.find_last_of("/\\") + 1);
            sprintf(label, "%s", shortN.c_str());
        }
        draw_list->AddText(ImVec2(dX1 + 5, dY1 + 5), IM_COL32(255, 255, 255, 255), label);
        
        clips[i].isInteracting = false;

        ImGui::SetCursorScreenPos(ImVec2(x1 - 4, currentY));
        ImGui::InvisibleButton((std::string("left_") + std::to_string(i)).c_str(), ImVec2(8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            clips[i].isInteracting = true;
            float delta = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].mediaStart + delta < 0.0f) delta = -clips[i].mediaStart;
            if (clips[i].timelineStart + delta >= clips[i].timelineEnd - 0.01f) delta = clips[i].timelineEnd - clips[i].timelineStart - 0.01f;
            clips[i].timelineStart += delta; clips[i].mediaStart += delta; 
            progress = clips[i].timelineStart; doSeek = true; selectedClipIndex = i;
        }

        ImGui::SetCursorScreenPos(ImVec2(x2 - 4, currentY));
        ImGui::InvisibleButton((std::string("right_") + std::to_string(i)).c_str(), ImVec2(8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            clips[i].isInteracting = true;
            float delta = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].mediaEnd + delta > 1.0f) delta = 1.0f - clips[i].mediaEnd;
            if (clips[i].timelineEnd + delta <= clips[i].timelineStart + 0.01f) delta = clips[i].timelineStart + 0.01f - clips[i].timelineEnd;
            clips[i].timelineEnd += delta; clips[i].mediaEnd += delta;
            progress = clips[i].timelineEnd; doSeek = true; selectedClipIndex = i;
        }

        ImGui::SetCursorScreenPos(ImVec2(x1 + 4, currentY));
        ImGui::InvisibleButton((std::string("body_") + std::to_string(i)).c_str(), ImVec2(x2 - x1 - 8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) selectedClipIndex = i;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            clips[i].isInteracting = true; 
            float deltaX = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].timelineStart + deltaX < 0.0f) deltaX = -clips[i].timelineStart;
            if (clips[i].timelineEnd + deltaX > 1.0f) deltaX = 1.0f - clips[i].timelineEnd;
            clips[i].timelineStart += deltaX; clips[i].timelineEnd += deltaX;
            progress = clips[i].timelineStart; doSeek = true;

            float mouse_y = ImGui::GetMousePos().y;
            int hoveredTrack = (mouse_y - p.y) / (trackHeight + trackSpacing);
            if (hoveredTrack >= 0 && hoveredTrack < tracks.size()) {
                if (tracks[hoveredTrack].type == tracks[clips[i].trackIndex].type) clips[i].trackIndex = hoveredTrack;
            }
        }
    }

    float playheadX = p.x + headerW + (progress * trackWidth);
    float playheadHeight = std::max(totalTimelineHeight, ImGui::GetWindowHeight());
    draw_list->AddLine(ImVec2(playheadX, p.y - 10), ImVec2(playheadX, p.y + playheadHeight), IM_COL32(255, 50, 50, 255), 2.0f);
    draw_list->AddTriangleFilled(ImVec2(playheadX - 6, p.y - 10), ImVec2(playheadX + 6, p.y - 10), ImVec2(playheadX, p.y), IM_COL32(255, 50, 50, 255));

    ImGui::SetCursorScreenPos(ImVec2(p.x + headerW, p.y));
    ImGui::InvisibleButton("##TrackArea", ImVec2(trackWidth, playheadHeight));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        progress = std::clamp((ImGui::GetMousePos().x - (p.x + headerW)) / trackWidth, 0.0f, 1.0f);
        doSeek = true;
    }

    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PROJECT_FILE")) {
            int fileIndex = *(const int*)payload->Data;
            std::string filepath = projectFiles[fileIndex];
            float dropTime = std::clamp((ImGui::GetMousePos().x - (p.x + headerW)) / trackWidth, 0.0f, 1.0f);
            float end = std::clamp(dropTime + 0.15f, 0.0f, 1.0f);
            int trackIdx = std::clamp(static_cast<int>((ImGui::GetMousePos().y - p.y) / (trackHeight + trackSpacing)), 0, static_cast<int>(tracks.size() - 1));
            clips.push_back(VideoClip(filepath, dropTime, end, trackIdx));
            selectedClipIndex = clips.size() - 1;
        }
        ImGui::EndDragDropTarget();
    }

    ImGui::EndChild();
    ImGui::End(); 
    return selectedFile;
}

void UIManager::DrawSurface(SDL_Renderer* renderer) { ImGui::Render(); ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer); }
void UIManager::Shutdown() { ImGui_ImplSDLRenderer2_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext(); }