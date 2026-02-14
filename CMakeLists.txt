cmake_minimum_required(VERSION 3.10)
project(VideoEditor)

set(CMAKE_CXX_STANDARD 17)

# --- 1. ПУТИ ---
set(LIBS_DIR "C:/Program/libs") 

include_directories("${LIBS_DIR}/SDL2/include")
include_directories("${LIBS_DIR}/ffmpeg/include")

# --- 2. СОЗДАЕМ EXE ---
add_executable(Editor main.cpp)

# --- 3. ЛИНКОВКА ---

# Библиотеки Windows, которые просит SDL2 (решают ошибки undefined reference)
set(WIN_LIBS setupapi imm32 version winmm gdi32)

# Подключаем всё вместе
target_link_libraries(Editor 
    mingw32 
    "${LIBS_DIR}/SDL2/lib/libSDL2main.a" 
    "${LIBS_DIR}/SDL2/lib/libSDL2.a"
    "${LIBS_DIR}/ffmpeg/lib/avformat.lib"
    "${LIBS_DIR}/ffmpeg/lib/avcodec.lib"
    "${LIBS_DIR}/ffmpeg/lib/avutil.lib"
    "${LIBS_DIR}/ffmpeg/lib/swscale.lib"
    ${WIN_LIBS}
)