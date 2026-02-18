#include "export_ui.h"
#include "imgui.h"
#include <windows.h>

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
    if (!*showMenu) return; // Рисуем окно, только если переменная true

    // Заставляем окно появиться по центру экрана
    ImGui::SetNextWindowPos(ImVec2(1280 / 2 - 200, 720 / 2 - 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Always);

    ImGui::Begin("Export Video", showMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    
    ImGui::Text("Render Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // 1. Выбор разрешения
    const char* resolutions[] = { "1920x1080 (Full HD)", "1280x720 (HD 720p)", "1080x1920 (TikTok/Shorts)" };
    static int resIndex = 1; // По умолчанию 720p
    if (ImGui::Combo("Resolution", &resIndex, resolutions, IM_ARRAYSIZE(resolutions))) {
        if (resIndex == 0) { width = 1920; height = 1080; }
        else if (resIndex == 1) { width = 1280; height = 720; }
        else if (resIndex == 2) { width = 1080; height = 1920; } // Вертикальное видео!
    }

    // 2. Выбор FPS
    ImGui::SliderInt("FPS", &fps, 24, 60);
    ImGui::Spacing();

    // 3. Куда сохранить файл
    ImGui::Text("Save to:");
    // Немного магии с буфером, чтобы ImGui мог показать строку
    char pathBuffer[260];
    strncpy(pathBuffer, outputPath.c_str(), sizeof(pathBuffer));
    ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("Browse...")) {
        std::string newPath = SaveFileDialog();
        if (!newPath.empty()) {
            if (newPath.find(".mp4") == std::string::npos) newPath += ".mp4"; // Дописываем формат
            outputPath = newPath;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Кнопка самого рендера
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Зеленая
    if (ImGui::Button("Start Render (BETA)", ImVec2(150, 40))) {
        // TODO: Здесь мы в будущем будем запускать FFmpeg Encoder!
        *showMenu = false; // Пока просто закрываем меню
    }
    ImGui::PopStyleColor();

    ImGui::End();
}