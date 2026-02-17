#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>

// НОВОЕ: Структура для хранения отрезков видео
struct VideoClip {
    std::string filepath;
    float startTime; // Начало отрезка (от 0.0 до 1.0)
    float endTime;   // Конец отрезка (от 0.0 до 1.0)
    float volume;    // Громкость (0.0 - 1.0)
};

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    // Передаем вектор клипов и индекс выбранного клипа
    std::string Render(int windowW, int windowH, int uiHeight, 
                       float& progress, bool& isPlaying, bool& doSeek,
                       std::vector<VideoClip>& clips, int& selectedClipIndex); 
                       
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
private:
    std::string OpenFileDialog();
};