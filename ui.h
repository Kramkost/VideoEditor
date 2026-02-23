#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

enum TrackType {
    TRACK_VIDEO,
    TRACK_AUDIO
};

struct TimelineTrack {
    std::string name;
    TrackType type;
};

struct VideoClip {
    std::string filepath;
    float timelineStart; 
    float timelineEnd;   
    float mediaStart;    
    float mediaEnd;      
    float volume;    
    int trackIndex; 
    
    // ПАРАМЕТРЫ ТРАНСФОРМАЦИИ
    float posX = 0.0f;     
    float posY = 0.0f;     
    float scale = 1.0f;    
    float rotation = 0.0f; 

    // TODO: [НОВОЕ] ПАРАМЕТРЫ ТЕКСТА
    bool isText = false;
    std::string textContent = "";
};

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    // Добавили bool& doAddText в аргументы
    std::string Render(int windowW, int windowH, int uiHeight, 
                       float& progress, bool& isPlaying, bool& doSeek,
                       std::vector<VideoClip>& clips, int& selectedClipIndex, 
                       bool& showExport, const std::vector<TimelineTrack>& tracks,
                       bool& doAddText);
                       
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
private:
    std::string OpenFileDialog();
};