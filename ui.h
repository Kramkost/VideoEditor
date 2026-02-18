#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

// TODO: [ВАЖНО] МАГИЯ МОНТАЖА НАЧИНАЕТСЯ ЗДЕСЬ!
// Теперь клип знает свое место на экране И свое место внутри исходного видео.
struct VideoClip {
    std::string filepath;
    // === ВРЕМЯ НА ТАЙМЛАЙНЕ (Где находится блок) ===
    float timelineStart; 
    float timelineEnd;   
    // === ВРЕМЯ ВНУТРИ ФАЙЛА (Какую часть видео играем) ===
    float mediaStart;    
    float mediaEnd;      
    
    float volume;    
};

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    std::string Render(int windowW, int windowH, int uiHeight, 
                       float& progress, bool& isPlaying, bool& doSeek,
                       std::vector<VideoClip>& clips, int& selectedClipIndex, 
                       bool& showExport);
                       
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
private:
    std::string OpenFileDialog();
};