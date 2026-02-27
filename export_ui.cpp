#include "export_ui.h"
#include "imgui.h"
#include <windows.h>
#include <cmath> // Для анимации sin()

std::string ExportUI::SaveFileDialog() {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Video Files\0*.mp4\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        return std::string(ofn.lpstrFile);
    }
    return "";
}

void ExportUI::Draw(bool* showMenu) {
    if (!*showMenu) return; 

    ImGui::SetNextWindowPos(ImVec2(1280 / 2 - 200, 720 / 2 - 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Always);

    ImGui::Begin("Export Video", showMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    
    ImGui::Text("Render Settings");
    ImGui::Separator();
    ImGui::Spacing();

    const char* resolutions[] = { "1920x1080 (Full HD)", "1280x720 (HD 720p)", "1080x1920 (TikTok/Shorts)" };
    static int resIndex = 1; 
    if (ImGui::Combo("Resolution", &resIndex, resolutions, IM_ARRAYSIZE(resolutions))) {
        if (resIndex == 0) { width = 1920; height = 1080; }
        else if (resIndex == 1) { width = 1280; height = 720; }
        else if (resIndex == 2) { width = 1080; height = 1920; } 
    }

    ImGui::SliderInt("FPS", &fps, 24, 60);
    ImGui::Spacing();

    ImGui::Text("Save to:");
    char pathBuffer[260];
    strncpy(pathBuffer, outputPath.c_str(), sizeof(pathBuffer));
    ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        std::string newPath = SaveFileDialog();
        if (!newPath.empty()) {
            if (newPath.find(".mp4") == std::string::npos) newPath += ".mp4"; 
            outputPath = newPath;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // --- АНИМАЦИЯ КНОПКИ РЕНДЕРА ---
    float time = ImGui::GetTime();
    float pulse = (std::sin(time * 5.0f) + 1.0f) * 0.5f; 
    ImVec4 btnColor = ImVec4(0.1f + 0.2f * pulse, 0.6f + 0.2f * pulse, 0.2f, 1.0f); // Пульсирующий зеленый
    
    ImGui::PushStyleColor(ImGuiCol_Button, btnColor); 
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
    
    if (ImGui::Button("START EXPORT", ImVec2(-1, 40))) {
        startRender = true; // Сигнал для main.cpp
        *showMenu = false;  // Закрываем окно настроек
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}