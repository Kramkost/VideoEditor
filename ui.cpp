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

std::string UIManager::Render(int windowW, int windowH, int uiHeight, float& progress, bool& isPlaying, bool& doSeek) {
    std::string selectedFile = "";
    
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, windowH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowW, uiHeight), ImGuiCond_Always);
    
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::Text("Titan Video Editor - Playback Controls");
    ImGui::Spacing();
    
    // НОВОЕ: Интерактивный ползунок! Если пользователь его двигает, мы сообщаем об этом через doSeek
    if (ImGui::SliderFloat("##Timeline", &progress, 0.0f, 1.0f, "%.3f")) {
        doSeek = true; 
    }
    ImGui::Spacing();
    
    if (ImGui::Button("Open Video")) { 
        selectedFile = OpenFileDialog();
    }
    ImGui::SameLine();
    
    // Умные кнопки Play/Pause
    if (isPlaying) {
        if (ImGui::Button("Pause")) isPlaying = false;
    } else {
        if (ImGui::Button("Play")) isPlaying = true;
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