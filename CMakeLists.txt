cmake_minimum_required(VERSION 3.10)
project(VideoEditor)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-DSDL_MAIN_HANDLED)

include_directories("imgui")
file(GLOB IMGUI_SOURCES "imgui/*.cpp")

if(WIN32)
    set(LIBS_DIR "C:/Program/libs")
    include_directories("${LIBS_DIR}/SDL2/include" "${LIBS_DIR}/SDL2/include/SDL2" "${LIBS_DIR}/ffmpeg/include")
    set(WIN_LIBS setupapi imm32 version winmm gdi32 comdlg32)
    set(SDL_LIBS "${LIBS_DIR}/SDL2/lib/libSDL2main.a" "${LIBS_DIR}/SDL2/lib/libSDL2.a" "${LIBS_DIR}/SDL2/lib/libSDL2_ttf.a")
    set(FFMPEG_LIBS "${LIBS_DIR}/ffmpeg/lib/avformat.lib" "${LIBS_DIR}/ffmpeg/lib/avcodec.lib" "${LIBS_DIR}/ffmpeg/lib/avutil.lib" "${LIBS_DIR}/ffmpeg/lib/swresample.lib" "${LIBS_DIR}/ffmpeg/lib/swscale.lib")
else()
    # Авто-поиск для Linux (Fedora)
    find_package(SDL2 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(AVCODEC REQUIRED libavcodec)
    pkg_check_modules(AVFORMAT REQUIRED libavformat)
    pkg_check_modules(SWSCALE REQUIRED libswscale)
    pkg_check_modules(AVUTIL REQUIRED libavutil)
    pkg_check_modules(SWRESAMPLE REQUIRED libswresample) 
    pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
    
    include_directories(${SDL2_INCLUDE_DIRS} ${AVCODEC_INCLUDE_DIRS} ${SDL2_TTF_INCLUDE_DIRS})
    set(SDL_LIBS SDL2::SDL2 ${SDL2_TTF_LIBRARIES})
    

    set(FFMPEG_LIBS ${AVCODEC_LIBRARIES} ${AVFORMAT_LIBRARIES} ${SWSCALE_LIBRARIES} ${AVUTIL_LIBRARIES} ${SWRESAMPLE_LIBRARIES})
endif()

add_executable(Editor main.cpp ui.cpp video_player.cpp export_ui.cpp project_manager.cpp ${IMGUI_SOURCES})
target_link_libraries(Editor ${SDL_LIBS} ${FFMPEG_LIBRARIES} ${FFMPEG_LIBS} ${WIN_LIBS})

# Плагины
add_library(blood_filter SHARED blood_filter.cpp)
set_target_properties(blood_filter PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins" PREFIX "")
