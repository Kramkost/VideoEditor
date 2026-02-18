#pragma once
#include <string>

class ExportUI {
public:
    void Draw(bool* showMenu); // Функция отрисовки всплывающего окна
    
    // Настройки, которые юзер выберет перед рендером
    std::string outputPath = "C:\\output.mp4";
    int width = 1280;
    int height = 720;
    int fps = 60;

private:
    std::string SaveFileDialog(); // Окно Windows для сохранения файла
};