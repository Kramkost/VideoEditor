#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include "ui.h" // Подключаем для доступа к VideoClip и TimelineTrack

// Структура, хранящая всё состояние текущего проекта
struct ProjectData {
    std::string projectName = "New Project";
    std::string saveFilepath = "";
    
    std::vector<TimelineTrack> tracks;
    std::vector<VideoClip> clips;
    std::vector<std::string> mediaFiles;

    void Clear() {
        projectName = "New Project";
        saveFilepath = "";
        tracks.clear();
        clips.clear();
        mediaFiles.clear();
    }
};

class ProjectManager {
public:
    static void Init();
    
    // Отрисовка стартового меню. Возвращает true, если проект выбран/создан и можно запускать редактор
    static bool DrawStartScreen(int windowW, int windowH, ProjectData& outProject);
    
    // Сохранение и загрузка в наш формат .titansave
    static bool SaveProject(const ProjectData& project);
    static bool SaveProjectAs(ProjectData& project); // Открывает диалог сохранения
    static bool LoadProject(const std::string& filepath, ProjectData& outProject);

private:
    static std::vector<std::string> recentProjects;
    static std::string projectToDelete;
    
    static void LoadRecentList();
    static void SaveRecentList();
    static void RemoveFromRecent(const std::string& filepath);
    
    static std::string OpenFileDialog();
    static std::string SaveFileDialog();
};