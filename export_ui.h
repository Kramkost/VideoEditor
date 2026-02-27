#pragma once
#include <string>

class ExportUI {
public:
    void Draw(bool* showMenu); 
    
    std::string outputPath = "C:\\output.mp4";
    int width = 1280;
    int height = 720;
    int fps = 60;
    
    // Флаг запуска рендера (main.cpp будет его считывать)
    bool startRender = false; 

private:
    std::string SaveFileDialog(); 
};