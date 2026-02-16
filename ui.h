#pragma once
#include <SDL2/SDL.h>
#include <string>

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    // Передаем переменные по ссылке (&), чтобы UI мог менять их в main.cpp
    std::string Render(int windowW, int windowH, int uiHeight, float& progress, bool& isPlaying, bool& doSeek); 
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
private:
    std::string OpenFileDialog();
};