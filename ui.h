#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

// TODO: [ВАЖНО] ТИПЫ ДОРОЖЕК
enum TrackType {
    TRACK_VIDEO,
    TRACK_AUDIO
};

// TODO: [ВАЖНО] СТРУКТУРА САМОЙ ДОРОЖКИ
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
    
    // TODO: [ВАЖНО] ИНДЕКС ДОРОЖКИ (0 = Video 1, 1 = Video 2, и т.д.)
    int trackIndex; 
};

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    // Обновили сигнатуру: теперь передаем список дорожек
    std::string Render(int windowW, int windowH, int uiHeight, 
                       float& progress, bool& isPlaying, bool& doSeek,
                       std::vector<VideoClip>& clips, int& selectedClipIndex, 
                       bool& showExport, const std::vector<TimelineTrack>& tracks);
                       
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
private:
    std::string OpenFileDialog();
};