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
#ifdef _WIN32
    #include <windows.h>
#endif
#include <iostream>
#include <cmath> 

std::vector<std::string> ProjectManager::recentProjects;
std::string ProjectManager::projectToDelete = "";

static std::string SerializeAnimTrack(const AnimTrack& track) {
    std::stringstream ss;
    ss << (track.isAnimated ? 1 : 0) << "," << static_cast<int>(track.easing) << "," << track.keys.size();
    for (const auto& k : track.keys) {
        ss << "," << k.time << "=" << k.value;
    }
    return ss.str();
}

static void DeserializeAnimTrack(const std::string& str, AnimTrack& track) {
    if (str.empty()) return;
    std::stringstream ss(str);
    std::string isAnimStr, easingStr, countStr;
    if (!std::getline(ss, isAnimStr, ',')) return;
    if (!std::getline(ss, easingStr, ',')) return;
    if (!std::getline(ss, countStr, ',')) return;

    track.isAnimated = (isAnimStr == "1");
    track.easing = static_cast<EasingType>(std::stoi(easingStr));
    int count = std::stoi(countStr);
    track.keys.clear();
    for (int i = 0; i < count; ++i) {
        std::string keyEntry;
        if (std::getline(ss, keyEntry, ',')) {
            size_t eq = keyEntry.find('=');
            if (eq != std::string::npos) {
                Keyframe k;
                k.time = std::stof(keyEntry.substr(0, eq));
                k.value = std::stof(keyEntry.substr(eq + 1));
                track.keys.push_back(k);
            }
        }
    }
}

void ProjectManager::Init() { LoadRecentList(); }

void ProjectManager::LoadRecentList() {
    recentProjects.clear();
    std::ifstream file("recent_projects.txt");
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && std::filesystem::exists(line)) recentProjects.push_back(line);
    }
}

void ProjectManager::SaveRecentList() {
    std::ofstream file("recent_projects.txt");
    for (const auto& path : recentProjects) file << path << "\n";
}

void ProjectManager::RemoveFromRecent(const std::string& filepath) {
    recentProjects.erase(std::remove(recentProjects.begin(), recentProjects.end(), filepath), recentProjects.end());
    SaveRecentList();
}

bool ProjectManager::DrawStartScreen(int windowW, int windowH, ProjectData& outProject) {
    bool projectReady = false;
    bool openDeletePopup = false; 

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(windowW, windowH));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("Start Screen", nullptr, flags);

    ImGui::TextColored(ImVec4(0.2f, 0.6f, 0.9f, 1.0f), "TITAN VIDEO EDITOR");
    ImGui::Separator(); ImGui::Spacing();

    ImGui::BeginChild("ActionsPanel", ImVec2(250, 0), true);
    if (ImGui::Button("Create New Project", ImVec2(-1, 50))) {
        outProject.Clear();
        outProject.tracks = { {"Video 1 (Main)", TRACK_VIDEO}, {"Video 2 (Overlay)", TRACK_VIDEO}, {"Audio 1 (Music)", TRACK_AUDIO} };
        projectReady = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Open Project", ImVec2(-1, 50))) {
        std::string file = OpenFileDialog();
        if (!file.empty() && LoadProject(file, outProject)) projectReady = true;
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RecentPanel", ImVec2(0, 0), true);
    ImGui::Text("Recent Projects"); ImGui::Separator();

    for (size_t i = 0; i < recentProjects.size(); ++i) {
        std::string filepath = recentProjects[i];
        std::string filename = std::filesystem::path(filepath).filename().string();
        ImGui::PushID(i);
        if (ImGui::Button(filename.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - 45, 40))) {
            if (LoadProject(filepath, outProject)) projectReady = true;
        }
        ImGui::SameLine();
        
        float time = ImGui::GetTime();
        float pulse = (std::sin(time * 6.0f) + 1.0f) * 0.5f; 
        ImVec4 hoverColor = ImVec4(0.7f + 0.3f * pulse, 0.1f, 0.1f, 1.0f); 
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));        
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);                     
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.0f, 0.0f, 1.0f));  
        if (ImGui::Button("X", ImVec2(40, 40))) { projectToDelete = filepath; openDeletePopup = true; }
        if (ImGui::IsItemHovered()) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); ImGui::SetTooltip("Delete Project permanently"); }
        ImGui::PopStyleColor(3); ImGui::PopID();
    }

    if (openDeletePopup) ImGui::OpenPopup("Delete Project?");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("Delete Project?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to delete this project?\n\n%s\n\nThis action cannot be undone!", projectToDelete.c_str());
        ImGui::Separator(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        if (ImGui::Button("YES, DELETE", ImVec2(120, 0))) {
            std::filesystem::remove(projectToDelete); 
            RemoveFromRecent(projectToDelete);        
            projectToDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SetItemDefaultFocus(); ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            projectToDelete.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
    return projectReady;
}

bool ProjectManager::SaveProjectAs(ProjectData& project) {
    project.saveFilepath = SaveFileDialog();
    if (project.saveFilepath.empty()) return false;
    return SaveProject(project);
}

bool ProjectManager::SaveProject(const ProjectData& project) {
    if (project.saveFilepath.empty()) return false;

    std::ofstream file(std::filesystem::path(project.saveFilepath));
    if (!file.is_open()) return false;

    // Сохраняем треки
    file << "[TRACKS]\n";
    for (const auto& t : project.tracks) {
        file << t.name << "|" << t.type << "\n";
    }

    file << "[MEDIA]\n";
    for (const auto& m : project.mediaFiles) file << m << "\n";

    file << "[CLIPS]\n";
    for (const auto& c : project.clips) {
        // Базовые поля
        file << c.id << "|" << c.parentId << "|" << c.isNullObject << "|" 
             << c.filepath << "|" << c.timelineStart << "|" << c.timelineEnd 
             << "|" << c.trackIndex << "|" << c.isText << "|" << c.textContent
             << "|" << c.volume << "|" << c.posX << "|" << c.posY 
             << "|" << c.scale << "|" << c.rotation
             << "|" << c.mediaStart << "|" << c.mediaEnd;
        
        // Эффекты (формат: count;name=intensity;name=intensity;...)
        file << "|" << c.effects.size();
        for (const auto& fx : c.effects) {
            file << ";" << fx.name << "=" << fx.intensity;
        }
        
        // Анимации
        file << "|" << SerializeAnimTrack(c.animX);
        file << "|" << SerializeAnimTrack(c.animY);
        file << "|" << SerializeAnimTrack(c.animScale);
        file << "|" << SerializeAnimTrack(c.animRot);
        
        file << "\n";
    }

    file.close();
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

    std::string line;
    std::string currentSection = "";
    bool tracksLoaded = false;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        if (line[0] == '[') { currentSection = line; continue; }

        try {
            if (currentSection == "[TRACKS]") {
                std::stringstream ss(line);
                std::string name, typeStr;
                std::getline(ss, name, '|');
                std::getline(ss, typeStr);
                if (!name.empty() && !typeStr.empty()) {
                    TrackType type = static_cast<TrackType>(std::stoi(typeStr));
                    outProject.tracks.push_back({name, type});
                    tracksLoaded = true;
                }
            }
            else if (currentSection == "[MEDIA]") {
                outProject.mediaFiles.push_back(line);
            }
            else if (currentSection == "[CLIPS]") {
                std::stringstream ss(line);
                std::string idStr, pIdStr, isNullStr, path, tStartStr, tEndStr, trackIdxStr, isTextStr, textContent;
                std::string volumeStr, posXStr, posYStr, scaleStr, rotationStr;
                std::string mediaStartStr, mediaEndStr, effectsBlock;
                
                // Базовые поля (обязательные)
                std::getline(ss, idStr, '|');
                std::getline(ss, pIdStr, '|');
                std::getline(ss, isNullStr, '|');
                std::getline(ss, path, '|');
                std::getline(ss, tStartStr, '|');
                std::getline(ss, tEndStr, '|');
                std::getline(ss, trackIdxStr, '|');
                std::getline(ss, isTextStr, '|');
                std::getline(ss, textContent, '|');

                if (idStr.empty() || tStartStr.empty() || tEndStr.empty() || trackIdxStr.empty()) continue;

                VideoClip clip(path, std::stof(tStartStr), std::stof(tEndStr), std::stoi(trackIdxStr), 
                               std::stoi(isTextStr), textContent, std::stoul(idStr), std::stoul(pIdStr), std::stoi(isNullStr));

                // Расширенные поля (опциональные — обратная совместимость со старыми файлами)
                if (std::getline(ss, volumeStr, '|') && !volumeStr.empty()) clip.volume = std::stof(volumeStr);
                if (std::getline(ss, posXStr, '|') && !posXStr.empty()) clip.posX = std::stof(posXStr);
                if (std::getline(ss, posYStr, '|') && !posYStr.empty()) clip.posY = std::stof(posYStr);
                if (std::getline(ss, scaleStr, '|') && !scaleStr.empty()) clip.scale = std::stof(scaleStr);
                if (std::getline(ss, rotationStr, '|') && !rotationStr.empty()) clip.rotation = std::stof(rotationStr);
                if (std::getline(ss, mediaStartStr, '|') && !mediaStartStr.empty()) clip.mediaStart = std::stof(mediaStartStr);
                if (std::getline(ss, mediaEndStr, '|') && !mediaEndStr.empty()) clip.mediaEnd = std::stof(mediaEndStr);
                
                // Эффекты
                if (std::getline(ss, effectsBlock, '|') && !effectsBlock.empty()) {
                    std::stringstream efxSS(effectsBlock);
                    std::string countStr;
                    if (std::getline(efxSS, countStr, ';') && !countStr.empty()) {
                        try {
                            int efxCount = std::stoi(countStr);
                            for (int e = 0; e < efxCount; ++e) {
                                std::string efxEntry;
                                if (std::getline(efxSS, efxEntry, ';')) {
                                    size_t eq = efxEntry.find('=');
                                    if (eq != std::string::npos) {
                                        clip.effects.push_back({efxEntry.substr(0, eq), std::stof(efxEntry.substr(eq + 1))});
                                    }
                                }
                            }
                        } catch (...) {}
                    }
                }
                
                // Анимации
                std::string animXStr, animYStr, animScaleStr, animRotStr;
                if (std::getline(ss, animXStr, '|')) DeserializeAnimTrack(animXStr, clip.animX);
                if (std::getline(ss, animYStr, '|')) DeserializeAnimTrack(animYStr, clip.animY);
                if (std::getline(ss, animScaleStr, '|')) DeserializeAnimTrack(animScaleStr, clip.animScale);
                if (std::getline(ss, animRotStr, '|')) DeserializeAnimTrack(animRotStr, clip.animRot);

                outProject.clips.push_back(clip);
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Failed to parse project line: " << e.what() << " | line: " << line << std::endl;
            continue; // Пропускаем битую строку вместо краша
        }
    }

    // Фоллбэк: если треки не были сохранены (старый формат)
    if (!tracksLoaded) {
        outProject.tracks = { {"Video 1 (Main)", TRACK_VIDEO}, {"Video 2 (Overlay)", TRACK_VIDEO}, {"Audio 1 (Music)", TRACK_AUDIO} };
    }

    // Восстанавливаем G_NextClipId, чтобы новые клипы не дублировали ID существующих
    uint32_t maxId = 0;
    for (const auto& clip : outProject.clips) {
        if (clip.id > maxId) maxId = clip.id;
    }
    G_NextClipId = maxId + 1;

    RemoveFromRecent(filepath);
    recentProjects.insert(recentProjects.begin(), filepath);
    SaveRecentList();
    return true;
}

std::string ProjectManager::OpenFileDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn; CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Titan Project (*.titansave)\0*.titansave\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
#else
    FILE* pipe = popen("zenity --file-selection --title=\"Open Project\" --file-filter=\"*.titansave\" 2>/dev/null", "r");
    if (pipe) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result = buffer;
            result.erase(result.find_last_not_of(" \n\r\t") + 1); 
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

std::string ProjectManager::SaveFileDialog() {
#ifdef _WIN32
    OPENFILENAMEA ofn; CHAR szFile[260] = {0};
    ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrDefExt = "titansave";
    ofn.lpstrFilter = "Titan Project (*.titansave)\0*.titansave\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn) == TRUE) return std::string(ofn.lpstrFile);
    return "";
#else
    FILE* pipe = popen("zenity --file-selection --save --title=\"Save Project\" --confirm-overwrite --file-filter=\"*.titansave\" 2>/dev/null", "r");
    if (pipe) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string result = buffer;
            result.erase(result.find_last_not_of(" \n\r\t") + 1); 
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}