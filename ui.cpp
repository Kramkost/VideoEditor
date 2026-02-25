#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h> 
#include <windows.h> 
#include <string>

void UIManager::Init(SDL_Window* window, SDL_Renderer* renderer) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
}

void UIManager::ProcessEvent(const SDL_Event* event) { ImGui_ImplSDL2_ProcessEvent(event); }

std::string UIManager::OpenFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Video Files\0*.mp4;*.mkv;*.avi;*.mov\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
}

std::string UIManager::Render(int windowW, int windowH, int uiHeight, 
                              float& progress, bool& isPlaying, bool& doSeek,
                              std::vector<VideoClip>& clips, int& selectedClipIndex,
                              bool& showExport, const std::vector<TimelineTrack>& tracks,
                              bool& doAddText, bool& effectChanged) {
    std::string selectedFile = "";
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int leftPanelWidth = 220;
    int rightPanelWidth = 300;

    ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < clips.size(); i++) {
        if (clips[i].isText && progress >= clips[i].timelineStart && progress < clips[i].timelineEnd) {
            
            // Вычисляем локальное время для анимации ТЕКСТА
            float len = clips[i].timelineEnd - clips[i].timelineStart;
            float localTime = (len > 0.001f) ? (progress - clips[i].timelineStart) / len : 0.0f;
            
            // ИСПОЛЬЗУЕМ КРИВЫЕ БЕЗЬЕ ДЛЯ ВЫЧИСЛЕНИЯ ПАРАМЕТРОВ!
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

    // === ЛЕВАЯ ПАНЕЛЬ (БИБЛИОТЕКА ЭФФЕКТОВ) ===
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(leftPanelWidth, windowH - uiHeight), ImGuiCond_Always);
    ImGui::Begin("Effects Library", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Video Plugins:");
    ImGui::Separator(); ImGui::Spacing();
    auto availablePlugins = PluginManager::GetAvailablePlugins();
    for (const auto& plugin : availablePlugins) {
        if (ImGui::Button(plugin.c_str(), ImVec2(-1, 35))) {
            if (selectedClipIndex >= 0 && selectedClipIndex < clips.size() && !clips[selectedClipIndex].isText) {
                clips[selectedClipIndex].effects.push_back({plugin, 1.0f}); 
                effectChanged = true;
            }
        }
    }
    ImGui::End();

    // === ПРАВАЯ ПАНЕЛЬ (ИНСПЕКТОР С АНИМАЦИЯМИ) ===
    ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - uiHeight), ImGuiCond_Always);
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::Text("Properties");
    ImGui::Separator();
    
    if (selectedClipIndex >= 0 && selectedClipIndex < clips.size()) {
        ImGui::Text("Selected Clip: %d", selectedClipIndex + 1);
        ImGui::Spacing();
        
        if (clips[selectedClipIndex].isText) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Text Content");
            char textBuf[256]; strncpy(textBuf, clips[selectedClipIndex].textContent.c_str(), sizeof(textBuf));
            if (ImGui::InputTextMultiline("##text", textBuf, sizeof(textBuf), ImVec2(-1, 50))) clips[selectedClipIndex].textContent = std::string(textBuf);
            ImGui::Spacing();
        }

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Transform & Animation");
        
        // ВЫЧИСЛЯЕМ ЛОКАЛЬНОЕ ВРЕМЯ (от 0.0 до 1.0)
        float len = clips[selectedClipIndex].timelineEnd - clips[selectedClipIndex].timelineStart;
        float localTime = (len > 0.001f) ? (progress - clips[selectedClipIndex].timelineStart) / len : 0.0f;
        if (localTime < 0.0f) localTime = 0.0f; if (localTime > 1.0f) localTime = 1.0f;

        // Хелпер: Отрисовка поля с кнопкой-секундомером
        auto DrawAnimParam = [&](const char* label, AnimTrack& track, float& baseVal, float speed, float minV, float maxV) {
            ImGui::PushID(label);
            ImVec4 btnColor = track.isAnimated ? ImVec4(0.2f, 0.6f, 0.9f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
            // Если нажимаем "O" - включаем/выключаем анимацию
            if (ImGui::Button(track.isAnimated ? " O " : " - ")) {
                track.isAnimated = !track.isAnimated;
                if (track.isAnimated) track.AddOrUpdateKey(localTime, baseVal);
                else track.keys.clear();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            
            float val = track.GetValue(localTime, baseVal);
            if (ImGui::DragFloat(label, &val, speed, minV, maxV)) {
                if (track.isAnimated) track.AddOrUpdateKey(localTime, val);
                else baseVal = val;
            }
            ImGui::PopID();
        };

        DrawAnimParam("Pos X", clips[selectedClipIndex].animX, clips[selectedClipIndex].posX, 1.0f, -3000, 3000);
        DrawAnimParam("Pos Y", clips[selectedClipIndex].animY, clips[selectedClipIndex].posY, 1.0f, -3000, 3000);
        DrawAnimParam("Scale", clips[selectedClipIndex].animScale, clips[selectedClipIndex].scale, 0.01f, 0.01f, 10.0f);
        if (!clips[selectedClipIndex].isText) DrawAnimParam("Rotation", clips[selectedClipIndex].animRot, clips[selectedClipIndex].rotation, 1.0f, -360.0f, 360.0f);
        ImGui::Spacing();

        if (!clips[selectedClipIndex].isText) {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "Applied Effects");
            ImGui::Separator();
            for (int e = 0; e < clips[selectedClipIndex].effects.size(); ++e) {
                auto& fx = clips[selectedClipIndex].effects[e];
                ImGui::PushID(e); ImGui::Text("%s", fx.name.c_str()); ImGui::SameLine(ImGui::GetWindowWidth() - 40);
                if (ImGui::Button("X")) { clips[selectedClipIndex].effects.erase(clips[selectedClipIndex].effects.begin() + e); effectChanged = true; ImGui::PopID(); break; }
                if (ImGui::SliderFloat("Intensity", &fx.intensity, 0.0f, 1.0f)) effectChanged = true;
                ImGui::PopID(); ImGui::Spacing();
            }
            ImGui::Spacing(); ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Audio");
            ImGui::SliderFloat("Volume", &clips[selectedClipIndex].volume, 0.0f, 1.0f, "%.2f");
        }
    } else ImGui::TextDisabled("Select a clip to edit properties.");
    ImGui::End();

    // === НИЖНЯЯ ПАНЕЛЬ (ТАЙМЛАЙН) ===
    ImGui::SetNextWindowPos(ImVec2(0, windowH - uiHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, uiHeight), ImGuiCond_Always);
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    
    if (ImGui::Button("Open Video")) { selectedFile = OpenFileDialog(); }
    ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f)); 
    if (ImGui::Button("+ ADD TEXT")) { doAddText = true; } ImGui::PopStyleColor();
    ImGui::SameLine();
    if (isPlaying) { if (ImGui::Button("Pause")) isPlaying = false; } else { if (ImGui::Button("Play")) isPlaying = true; }
    
    ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); 
    if (ImGui::Button("CUT (Split here)")) {
        if (clips.size() > 0 && selectedClipIndex >= 0) {
            VideoClip newClip = clips[selectedClipIndex];
            float ratio = (progress - clips[selectedClipIndex].timelineStart) / (clips[selectedClipIndex].timelineEnd - clips[selectedClipIndex].timelineStart);
            float splitMedia = clips[selectedClipIndex].mediaStart + ratio * (clips[selectedClipIndex].mediaEnd - clips[selectedClipIndex].mediaStart);
            newClip.timelineStart = progress; newClip.mediaStart = splitMedia;
            clips[selectedClipIndex].timelineEnd = progress; clips[selectedClipIndex].mediaEnd = splitMedia;
            clips.insert(clips.begin() + selectedClipIndex + 1, newClip);
            selectedClipIndex++; 
        }
    } ImGui::PopStyleColor();

    ImGui::SameLine(); if (clips.empty()) ImGui::BeginDisabled(); 
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f)); 
    if (ImGui::Button("EXPORT VIDEO")) showExport = true; 
    ImGui::PopStyleColor(); if (clips.empty()) ImGui::EndDisabled(); 

    ImGui::Spacing(); ImGui::Separator();
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float trackWidth = ImGui::GetContentRegionAvail().x;
    float trackHeight = 40.0f; float trackSpacing = 4.0f;
    float totalTimelineHeight = tracks.size() * (trackHeight + trackSpacing);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    for (int t = 0; t < tracks.size(); ++t) {
        float currentY = p.y + t * (trackHeight + trackSpacing);
        ImU32 bgColor = (tracks[t].type == TRACK_VIDEO) ? IM_COL32(40, 40, 45, 255) : IM_COL32(35, 45, 40, 255);
        draw_list->AddRectFilled(ImVec2(p.x, currentY), ImVec2(p.x + trackWidth, currentY + trackHeight), bgColor);
        draw_list->AddText(ImVec2(p.x + 5, currentY + 5), IM_COL32(150, 150, 150, 255), tracks[t].name.c_str());
    }

    for (int i = 0; i < clips.size(); i++) {
        int t = clips[i].trackIndex; if (t < 0 || t >= tracks.size()) continue; 

        float currentY = p.y + t * (trackHeight + trackSpacing);
        float x1 = p.x + (clips[i].timelineStart * trackWidth);
        float x2 = p.x + (clips[i].timelineEnd * trackWidth);
        
        ImU32 clipColor = (i == selectedClipIndex) ? IM_COL32(100, 150, 220, 255) : IM_COL32(50, 100, 160, 255);
        if (tracks[t].type == TRACK_AUDIO && i != selectedClipIndex) clipColor = IM_COL32(50, 160, 100, 255); 
        if (clips[i].isText && i != selectedClipIndex) clipColor = IM_COL32(160, 100, 50, 255); 
        
        draw_list->AddRectFilled(ImVec2(x1 + 1, currentY + 2), ImVec2(x2 - 1, currentY + trackHeight - 2), clipColor, 4.0f);
        
        // TODO: [КРАСОТА] Рисуем ключи-ромбики прямо на клипе, если включена анимация!
        auto DrawDiamonds = [&](const AnimTrack& track, ImU32 color) {
            if (!track.isAnimated) return;
            for (const auto& k : track.keys) {
                float kx = p.x + (clips[i].timelineStart + k.time * (clips[i].timelineEnd - clips[i].timelineStart)) * trackWidth;
                float ky = currentY + trackHeight - 8.0f;
                ImVec2 p1(kx, ky - 4), p2(kx + 4, ky), p3(kx, ky + 4), p4(kx - 4, ky);
                ImVec2 quad[4] = {p1, p2, p3, p4};
                draw_list->AddConvexPolyFilled(quad, 4, color);
            }
        };
        // Ромбики для позиции и масштаба
        DrawDiamonds(clips[i].animX, IM_COL32(255, 200, 50, 255)); 
        DrawDiamonds(clips[i].animScale, IM_COL32(50, 255, 100, 255));

        char label[64]; 
        if (clips[i].isText) sprintf(label, "Text: %s", clips[i].textContent.c_str());
        else sprintf(label, "Clip %d", i + 1);
        draw_list->AddText(ImVec2(x1 + 5, currentY + 5), IM_COL32(255, 255, 255, 255), label);
        
        ImGui::SetCursorScreenPos(ImVec2(x1 - 4, currentY));
        ImGui::InvisibleButton((std::string("left_") + std::to_string(i)).c_str(), ImVec2(8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
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
            float deltaX = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].timelineStart + deltaX < 0.0f) deltaX = -clips[i].timelineStart;
            if (clips[i].timelineEnd + deltaX > 1.0f) deltaX = 1.0f - clips[i].timelineEnd;
            clips[i].timelineStart += deltaX; clips[i].timelineEnd += deltaX;
            progress = clips[i].timelineStart; doSeek = true;

            float mouse_y = ImGui::GetMousePos().y;
            int hoveredTrack = (mouse_y - p.y) / (trackHeight + trackSpacing);
            if (hoveredTrack >= 0 && hoveredTrack < tracks.size()) {
                if (tracks[hoveredTrack].type == tracks[clips[i].trackIndex].type) {
                    clips[i].trackIndex = hoveredTrack;
                }
            }
        }
    }

    float playheadX = p.x + (progress * trackWidth);
    draw_list->AddLine(ImVec2(playheadX, p.y - 10), ImVec2(playheadX, p.y + totalTimelineHeight + 10), IM_COL32(255, 50, 50, 255), 2.0f);
    draw_list->AddTriangleFilled(ImVec2(playheadX - 6, p.y - 10), ImVec2(playheadX + 6, p.y - 10), ImVec2(playheadX, p.y), IM_COL32(255, 50, 50, 255));

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y));
    ImGui::InvisibleButton("##TrackArea", ImVec2(trackWidth, totalTimelineHeight));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        progress = (ImGui::GetMousePos().x - p.x) / trackWidth;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        doSeek = true;
    }

    ImGui::End();
    return selectedFile;
}

void UIManager::DrawSurface(SDL_Renderer* renderer) { ImGui::Render(); ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer); }
void UIManager::Shutdown() { ImGui_ImplSDLRenderer2_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext(); }