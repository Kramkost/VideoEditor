#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <stdio.h> 
#include <windows.h> 

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

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

std::string UIManager::Render(int windowW, int windowH, int uiHeight, 
                              float& progress, bool& isPlaying, bool& doSeek,
                              std::vector<VideoClip>& clips, int& selectedClipIndex) {
    std::string selectedFile = "";
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    // === ПРАВАЯ ПАНЕЛЬ (Инспектор / Эквалайзер) ===
    int rightPanelWidth = 300;
    ImGui::SetNextWindowPos(ImVec2(windowW - rightPanelWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, windowH - uiHeight), ImGuiCond_Always);
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::Text("Audio Properties");
    ImGui::Separator();
    
    if (selectedClipIndex >= 0 && selectedClipIndex < clips.size()) {
        ImGui::Text("Clip: Segment %d", selectedClipIndex + 1);
        ImGui::Spacing();
        // Ползунок громкости для конкретного клипа
        ImGui::SliderFloat("Volume", &clips[selectedClipIndex].volume, 0.0f, 1.0f, "%.2f");
        
        // Заготовка под графический эквалайзер
        ImGui::Spacing();
        ImGui::Text("Equalizer (Coming soon)");
        float arr[] = { 0.2f, 0.5f, 0.8f, 0.4f, 0.9f, 0.3f, 0.6f };
        ImGui::PlotHistogram("##EQ", arr, IM_ARRAYSIZE(arr), 0, NULL, 0.0f, 1.0f, ImVec2(0, 80));
    } else {
        ImGui::TextDisabled("Select a clip to edit properties.");
    }
    ImGui::End();

    // === НИЖНЯЯ ПАНЕЛЬ (Таймлайн) ===
    ImGui::SetNextWindowPos(ImVec2(0, windowH - uiHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, uiHeight), ImGuiCond_Always);
    
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);
    ImGui::Text("Titan Video Editor - Timeline");
    ImGui::Spacing();
    
    if (ImGui::SliderFloat("##Timeline", &progress, 0.0f, 1.0f, "%.3f")) {
        doSeek = true; 
    }
    ImGui::Spacing();
    
    if (ImGui::Button("Open Video")) { selectedFile = OpenFileDialog(); }
    ImGui::SameLine();
    
    if (isPlaying) {
        if (ImGui::Button("Pause")) isPlaying = false;
    } else {
        if (ImGui::Button("Play")) isPlaying = true;
    }
    ImGui::SameLine();
    
    // НОВОЕ: Кнопка CUT
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f)); // Красная кнопка
    if (ImGui::Button("CUT (Split here)")) {
        // Логика разрезания (создаем новый клип в массиве)
        if (clips.size() > 0 && selectedClipIndex >= 0) {
            VideoClip newClip = clips[selectedClipIndex];
            newClip.startTime = progress; // Новый клип начинается там, где ползунок
            clips[selectedClipIndex].endTime = progress; // Старый заканчивается тут
            
            clips.insert(clips.begin() + selectedClipIndex + 1, newClip);
            selectedClipIndex++; // Выбираем новый кусок
        }
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Segments:");
    
    // Отрисовываем наши кусочки (клипы)
    for (int i = 0; i < clips.size(); i++) {
        if (i > 0) ImGui::SameLine();
        
        // Меняем цвет кнопки, если она выбрана
        if (i == selectedClipIndex) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
        
        char btnLabel[32];
        sprintf(btnLabel, "Clip %d", i + 1);
        if (ImGui::Button(btnLabel, ImVec2(80, 30))) {
            selectedClipIndex = i; // Выбираем клип при клике
            // По-хорошему, тут еще нужно прыгнуть на время clips[i].startTime
            progress = clips[i].startTime;
            doSeek = true;
        }
        
        if (i == selectedClipIndex) ImGui::PopStyleColor();
    }
    
    ImGui::End();
    ImGui::Render();
    
    return selectedFile;
}

void UIManager::DrawSurface(SDL_Renderer* renderer) {
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void UIManager::Shutdown() {
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
}