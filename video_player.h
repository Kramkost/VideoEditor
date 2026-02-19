#pragma once
#include <string>
#include <SDL2/SDL.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>             
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h> 
}

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool LoadVideo(const std::string& filepath, SDL_Renderer* renderer);
    void CloseVideo();
    
    // НОВОЕ: Добавлен параметр drawVideo (Рисовать ли кадр на экран)
    void UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH, double targetTimeSec, bool drawVideo);
    
    float GetProgress();
    void Seek(float progress);
    void ClearAudio();
    double GetDurationSeconds();

    bool isPlaying = false;
    float currentVolume = 1.0f;
    std::string loadedFilepath = ""; // НОВОЕ: Запоминаем, какой файл сейчас внутри
    bool isLoaded = false;           // Вынесли в public для проверок

private:
    double durationSec = 0;
    double currentSec = 0;
    
    AVFormatContext* formatCtx = nullptr;
    
    AVCodecContext* videoCodecCtx = nullptr; 
    int videoStreamIndex = -1;
    SwsContext* sws_ctx = nullptr;
    AVFrame* pFrame = nullptr;
    AVFrame* pFrameBGR = nullptr;
    uint8_t* videoBuffer = nullptr;
    SDL_Texture* texture = nullptr;
    
    AVCodecContext* audioCodecCtx = nullptr;
    int audioStreamIndex = -1;
    SwrContext* swrCtx = nullptr;
    AVFrame* aFrame = nullptr;
    SDL_AudioDeviceID audioDevice = 0;
    uint8_t* audioBuffer = nullptr;
    
    AVPacket* pPacket = nullptr;
};