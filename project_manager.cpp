/* =========================================================================
 * PROJECT MANAGER MODULE
 * =========================================================================
 * This file handles all project-related operations:
 * - Rendering the Start Screen UI.
 * - Loading and Saving project files (.titansave) using custom serialization.
 * - Managing the "Recent Projects" list.
 * * FIXES & ADDITIONS:
 * - Fixed the "Delete Project" modal not appearing (ImGui ID stack issue).
 * - Added a pulsing color animation when hovering over the delete (X) button.
 * - Added tooltip and cursor change on hover for better UX.
 * ========================================================================= */

#include "project_manager.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <windows.h>
#include <iostream>
#include <cmath> // Required for sin() animation

std::vector<std::string> ProjectManager::recentProjects;
std::string ProjectManager::projectToDelete = "";

void ProjectManager::Init() {
    LoadRecentList();
}

void ProjectManager::LoadRecentList() {
    recentProjects.clear();
    std::ifstream file("recent_projects.txt");
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && std::filesystem::exists(line)) {
            recentProjects.push_back(line);
        }
    }
}

void ProjectManager::SaveRecentList() {
    std::ofstream file("recent_projects.txt");
    for (const auto& path : recentProjects) {
        file << path << "\n";
    }
}

void ProjectManager::RemoveFromRecent(const std::string& filepath) {
    recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), filepath), recentProjects.end());
    SaveRecentList();
}

bool ProjectManager::DrawStartScreen(int windowW, int windowH, ProjectData& outProject) {
    bool projectReady = false;
    bool openDeletePopup = false; // FLAG FIX: Used to open popup outside the PushID loop

    // Fullscreen window setup
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(windowW, windowH));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Start Screen", nullptr, flags);

    ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.9f, 1.0f), "TITAN VIDEO EDITOR");
    ImGui::Separator();
    ImGui::Spacing();

    // --- LEFT PANEL: CREATE & OPEN ---
    ImGui::BeginChild("ActionsPanel", ImVec2(250, 0), true);
    if (ImGui::Button("Create New Project", ImVec2(-1, 50))) {
        outProject.Clear();
        // Setup default tracks
        outProject.tracks = {
            {"Video 1 (Main)", TRACK_VIDEO},
            {"Video 2 (Overlay)", TRACK_VIDEO},
            {"Audio 1 (Music)", TRACK_AUDIO}
        };
        projectReady = true;
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Open Project", ImVec2(-1, 50))) {
        std::string file = OpenFileDialog();
        if (!file.empty() && LoadProject(file, outProject)) {
            projectReady = true;
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // --- RIGHT PANEL: RECENT PROJECTS ---
    ImGui::BeginChild("RecentPanel", ImVec2(0, 0), true);
    ImGui::Text("Recent Projects");
    ImGui::Separator();

    for (size_t i = 0; i < recentProjects.size(); ++i) {
        std::string filepath = recentProjects[i];
        std::string filename = std::filesystem::path(filepath).filename().string();

        ImGui::PushID(i);
        
        // Load Project Button
        if (ImGui::Button(filename.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 45, 40))) {
            if (LoadProject(filepath, outProject)) {
                projectReady = true;
            }
        }
        ImGui::SameLine();
        
        // --- ANIMATED DELETE BUTTON ---
        // Creating a pulsing red effect using ImGui::GetTime()
        float time = ImGui::GetTime();
        float pulse = (std::sin(time * 6.0f) + 1.0f) * 0.5f; // Value oscillates between 0.0 and 1.0
        ImVec4 hoverColor = ImVec4(0.7f + 0.3f * pulse, 0.1f, 0.1f, 1.0f); // Pulses bright red
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));        // Normal color
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);                     // Animated hover color
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.0f, 0.0f, 1.0f));  // Click color
        
        if (ImGui::Button("X", ImVec2(40, 40))) {
            projectToDelete = filepath;
            openDeletePopup = true; // TRIGGER FLAG
        }
        
        // Extra UX: Tooltip and Cursor change on hover
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("Delete Project permanently");
        }
        
        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    // --- POPUP TRIGGER ---
    // Safely open the popup outside the PushID loop!
    if (openDeletePopup) {
        ImGui::OpenPopup("Delete Project?");
    }

    // --- DELETE CONFIRMATION MODAL ---
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Project?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this project?\n\n%s\n\nThis action cannot be undone!", projectToDelete.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("YES, DELETE", ImVec2(120, 0))) {
            std::filesystem::remove(projectToDelete); // Delete file from disk
            RemoveFromRecent(projectToDelete);        // Remove from list
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();

    return projectReady;
}

// --- CORE SERIALIZATION LOGIC ---
bool ProjectManager::SaveProjectAs(ProjectData& project) {
    project.saveFilepath = SaveFileDialog();
    if (project.saveFilepath.empty()) return false;
    return SaveProject(project);
}

bool ProjectManager::SaveProject(const ProjectData& project) {
    if (project.saveFilepath.empty()) return false;

    // ИСПОЛЬЗУЕМ std::filesystem::path ДЛЯ ЗАЩИТЫ ОТ БАГА С РУССКИМИ БУКВАМИ
    std::ofstream file(std::filesystem::path(project.saveFilepath));
    if (!file.is_open()) {
        std::cerr << "ОШИБКА: Не удалось открыть файл для сохранения! (Возможно, проблема с кодировкой пути): " << project.saveFilepath << "\n";
        return false;
    }

    // Сохраняем медиа файлы
    file << "[MEDIA]\n";
    for (const auto& m : project.mediaFiles) file << m << "\n";

    // Сохраняем клипы (Здесь можно расширить для анимаций и эффектов)
    file << "[CLIPS]\n";
    for (const auto& c : project.clips) {
        file << c.filepath << "|" << c.timelineStart << "|" << c.timelineEnd << "|" << c.trackIndex << "|" << c.isText << "|" << c.textContent << "\n";
    }

    file.close();

    // Добавляем в недавние
    if (std::find(recentProjects.begin(), recentProjects.end(), project.saveFilepath) == recentProjects.end()) {
        recentProjects.insert(recentProjects.begin(), project.saveFilepath);
        SaveRecentList();
    }

    return true;
}

bool ProjectManager::LoadProject(const std::string& filepath, ProjectData& outProject) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    outProject.Clear();
    outProject.saveFilepath = filepath;
    outProject.projectName = std::filesystem::path(filepath).stem().string();
    
    outProject.tracks = { {"Video 1 (Main)", TRACK_VIDEO}, {"Video 2 (Overlay)", TRACK_VIDEO}, {"Audio 1 (Music)", TRACK_AUDIO} };

    std::string line;
    std::string currentSection = "";

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line[0] == '[') { currentSection = line; continue; }

        if (currentSection == "[MEDIA]") {
            outProject.mediaFiles.push_back(line);
        }
        else if (currentSection == "[CLIPS]") {
            std::stringstream ss(line);
            std::string path, tStartStr, tEndStr, trackIdxStr, isTextStr, textContent;
            
            std::getline(ss, path, '|');
            std::getline(ss, tStartStr, '|');
            std::getline(ss, tEndStr, '|');
            std::getline(ss, trackIdxStr, '|');
            std::getline(ss, isTextStr, '|');
            std::getline(ss, textContent);

            VideoClip clip(path, std::stof(tStartStr), std::stof(tEndStr), std::stoi(trackIdxStr), std::stoi(isTextStr), textContent);
            outProject.clips.push_back(clip);
        }
    }

    RemoveFromRecent(filepath);
    recentProjects.insert(recentProjects.begin(), filepath);
    SaveRecentList();

    return true;
}

std::string ProjectManager::OpenFileDialog() {
    OPENFILENAMEA ofn; CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Titan Project (*.titansave)\0*.titansave\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
}

std::string ProjectManager::SaveFileDialog() {
    OPENFILENAMEA ofn; CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrDefExt = "titansave";
    ofn.lpstrFilter = "Titan Project (*.titansave)\0*.titansave\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
}