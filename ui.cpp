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

void UIManager::ProcessEvent(const SDL_Event* event) {
    ImGui_ImplSDL2_ProcessEvent(event);
}

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

    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
}

std::string UIManager::Render(int windowW, int windowH, int uiHeight, 
                              float& progress, bool& isPlaying, bool& doSeek,
                              std::vector<VideoClip>& clips, int& selectedClipIndex,
                                bool& showExport) {
    std::string selectedFile = "";
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int rightPanelWidth = 300;
    ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - uiHeight), ImGuiCond_Always);
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::Text("Audio Properties");
    ImGui::Separator();
    
    if (selectedClipIndex >= 0 && selectedClipIndex < clips.size()) {
        ImGui::Text("Clip: Segment %d", selectedClipIndex + 1);
        ImGui::Spacing();
        ImGui::SliderFloat("Volume", &clips[selectedClipIndex].volume, 0.0f, 1.0f, "%.2f");
    } else {
        ImGui::TextDisabled("Select a clip to edit properties.");
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(0, windowH - uiHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, uiHeight), ImGuiCond_Always);
    
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    
    if (ImGui::Button("Open Video")) { selectedFile = OpenFileDialog(); }
    ImGui::SameLine();
    if (isPlaying) { if (ImGui::Button("Pause")) isPlaying = false; } 
    else { if (ImGui::Button("Play")) isPlaying = true; }
    
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); 
    
    // TODO: [ВАЖНО] ЛОГИКА РАЗРЕЗАНИЯ (CUT) ПО НОВЫМ КООРДИНАТАМ
    if (ImGui::Button("CUT (Split here)")) {
        if (clips.size() > 0 && selectedClipIndex >= 0) {
            VideoClip newClip = clips[selectedClipIndex];
            
            // Вычисляем, на какой секунде самого МЕДИА мы сейчас стоим
            float ratio = (progress - clips[selectedClipIndex].timelineStart) / (clips[selectedClipIndex].timelineEnd - clips[selectedClipIndex].timelineStart);
            float splitMedia = clips[selectedClipIndex].mediaStart + ratio * (clips[selectedClipIndex].mediaEnd - clips[selectedClipIndex].mediaStart);

            newClip.timelineStart = progress;
            newClip.mediaStart = splitMedia;

            clips[selectedClipIndex].timelineEnd = progress;
            clips[selectedClipIndex].mediaEnd = splitMedia;

            clips.insert(clips.begin() + selectedClipIndex + 1, newClip);
            selectedClipIndex++; 
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f)); // Синяя кнопка
    if (ImGui::Button("EXPORT VIDEO")) {
        showExport = true; // Открываем всплывающее окно
    }
    ImGui::PopStyleColor();

    ImGui::Spacing(); ImGui::Separator();
    ImGui::Text("Video Track 1");
    
    ImVec2 p = ImGui::GetCursorScreenPos();
    float trackWidth = ImGui::GetContentRegionAvail().x;
    float trackHeight = 50.0f; 
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(p, ImVec2(p.x + trackWidth, p.y + trackHeight), IM_COL32(30, 30, 30, 255));

    for (int i = 0; i < clips.size(); i++) {
        float x1 = p.x + (clips[i].timelineStart * trackWidth);
        float x2 = p.x + (clips[i].timelineEnd * trackWidth);
        
        ImU32 clipColor = (i == selectedClipIndex) ? IM_COL32(100, 150, 220, 255) : IM_COL32(50, 100, 160, 255);
        draw_list->AddRectFilled(ImVec2(x1 + 1, p.y + 2), ImVec2(x2 - 1, p.y + trackHeight - 2), clipColor, 4.0f);
        
        char label[32]; sprintf(label, "Clip %d", i + 1);
        draw_list->AddText(ImVec2(x1 + 5, p.y + 5), IM_COL32(255, 255, 255, 255), label);
        
        // TODO: [ВАЖНО] ЛЕВЫЙ КРАЙ (TRIM IN)
        ImGui::SetCursorScreenPos(ImVec2(x1 - 4, p.y));
        ImGui::InvisibleButton((std::string("left_") + std::to_string(i)).c_str(), ImVec2(8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float delta = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].mediaStart + delta < 0.0f) delta = -clips[i].mediaStart;
            if (clips[i].timelineStart + delta >= clips[i].timelineEnd - 0.01f) delta = clips[i].timelineEnd - clips[i].timelineStart - 0.01f;
            
            clips[i].timelineStart += delta;
            clips[i].mediaStart += delta; // Время медиа едет вместе с краем!
            
            progress = clips[i].timelineStart; doSeek = true; selectedClipIndex = i;
        }

        // TODO: [ВАЖНО] ПРАВЫЙ КРАЙ (TRIM OUT)
        ImGui::SetCursorScreenPos(ImVec2(x2 - 4, p.y));
        ImGui::InvisibleButton((std::string("right_") + std::to_string(i)).c_str(), ImVec2(8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float delta = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].mediaEnd + delta > 1.0f) delta = 1.0f - clips[i].mediaEnd;
            if (clips[i].timelineEnd + delta <= clips[i].timelineStart + 0.01f) delta = clips[i].timelineStart + 0.01f - clips[i].timelineEnd;
            
            clips[i].timelineEnd += delta;
            clips[i].mediaEnd += delta;
            
            progress = clips[i].timelineEnd; doSeek = true; selectedClipIndex = i;
        }

        // TODO: [ВАЖНО] ПЕРЕМЕЩЕНИЕ КЛИПА ЦЕЛИКОМ (DRAG & DROP)
        ImGui::SetCursorScreenPos(ImVec2(x1 + 4, p.y));
        ImGui::InvisibleButton((std::string("body_") + std::to_string(i)).c_str(), ImVec2(x2 - x1 - 8, trackHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        
        if (ImGui::IsItemClicked()) selectedClipIndex = i;
        
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            float delta = ImGui::GetIO().MouseDelta.x / trackWidth;
            if (clips[i].timelineStart + delta < 0.0f) delta = -clips[i].timelineStart;
            if (clips[i].timelineEnd + delta > 1.0f) delta = 1.0f - clips[i].timelineEnd;
            
            clips[i].timelineStart += delta;
            clips[i].timelineEnd += delta;
            // Время МЕДИА здесь не меняется, мы просто двигаем "окно" в другое место!
            
            progress = clips[i].timelineStart; doSeek = true;
        }
    }

    float playheadX = p.x + (progress * trackWidth);
    draw_list->AddLine(ImVec2(playheadX, p.y - 10), ImVec2(playheadX, p.y + trackHeight + 10), IM_COL32(255, 50, 50, 255), 2.0f);
    draw_list->AddTriangleFilled(ImVec2(playheadX - 6, p.y - 10), ImVec2(playheadX + 6, p.y - 10), ImVec2(playheadX, p.y), IM_COL32(255, 50, 50, 255));

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + trackHeight));
    ImGui::InvisibleButton("##TrackArea", ImVec2(trackWidth, 20.0f));
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        progress = (ImGui::GetMousePos().x - p.x) / trackWidth;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        doSeek = true;
    }

    ImGui::End();
    return selectedFile;
}

void UIManager::DrawSurface(SDL_Renderer* renderer) {
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void UIManager::Shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}

