cmake_minimum_required(VERSION 3.10) # 
project(VideoEditor)

set(CMAKE_CXX_STANDARD 17)

# --- 1. ПУТИ ---
set(LIBS_DIR "C:/Program/libs")

include_directories("${LIBS_DIR}/SDL2/include")
include_directories("${LIBS_DIR}/SDL2/include/SDL2")
include_directories("${LIBS_DIR}/ffmpeg/include")
include_directories("imgui") # NEW: Путь к заголовочным файлам ImGui

# NEW: Собираем все .cpp файлы ImGui в одну переменную
file(GLOB IMGUI_SOURCES "imgui/*.cpp") 

# --- 2. СОЗДАЕМ EXE ---
# Строка создания exe:
add_executable(Editor main.cpp ui.cpp video_player.cpp export_ui.cpp project_manager.cpp ${IMGUI_SOURCES})

# Строка WIN_LIBS (добавили comdlg32 в конец):
set(WIN_LIBS setupapi imm32 version winmm gdi32 comdlg32)

target_link_libraries(Editor 
    mingw32 
    "${LIBS_DIR}/SDL2/lib/libSDL2main.a" 
    "${LIBS_DIR}/SDL2/lib/libSDL2.a"
    "${LIBS_DIR}/ffmpeg/lib/avformat.lib"
    "${LIBS_DIR}/ffmpeg/lib/avcodec.lib"
    "${LIBS_DIR}/ffmpeg/lib/avutil.lib"
    "${LIBS_DIR}/ffmpeg/lib/swresample.lib"
    "${LIBS_DIR}/ffmpeg/lib/swscale.lib"
    ${WIN_LIBS}
)

# --- 3. СИСТЕМА ПЛАГИНОВ (АВТОМАТИЧЕСКАЯ СБОРКА DLL) ---
# Говорим CMake собрать blood_filter.cpp как разделяемую библиотеку (SHARED = DLL)
add_library(blood_filter SHARED blood_filter.cpp)

# Настраиваем, чтобы DLL сохранялась прямо в папку plugins рядом с Editor.exe
set_target_properties(blood_filter PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    PREFIX "" # Убираем авто-префикс "lib", чтобы файл назывался именно blood_filter.dll
)