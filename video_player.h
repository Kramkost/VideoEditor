#pragma once
#include <string>
#include <SDL2/SDL.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>             // Чтобы компилятор знал старые функции (на всякий)
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h> // Звуки и рессемплер
}

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool LoadVideo(const std::string& filepath, SDL_Renderer* renderer);
    void CloseVideo();
    void UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH, double targetTimeSec);
    
    float GetProgress();
    void Seek(float progress);
    void ClearAudio();
    double GetDurationSeconds();
    bool isPlaying = false;
    float currentVolume = 1.0f;

private:
    double timeBase = 0;
    AVFormatContext* formatCtx = nullptr;
    
    // Видео переменные
    AVCodecContext* videoCodecCtx = nullptr; // Переименовал для понятности
    int videoStreamIndex = -1;
    SwsContext* sws_ctx = nullptr;
    AVFrame* pFrame = nullptr;
    AVFrame* pFrameBGR = nullptr;
    uint8_t* videoBuffer = nullptr;
    SDL_Texture* texture = nullptr;
    
    // НОВОЕ: Аудио переменные
    AVCodecContext* audioCodecCtx = nullptr;
    int audioStreamIndex = -1;
    SwrContext* swrCtx = nullptr;
    AVFrame* aFrame = nullptr;
    SDL_AudioDeviceID audioDevice = 0;
    uint8_t* audioBuffer = nullptr;
    
    AVPacket* pPacket = nullptr;
    
    bool isLoaded = false;
    double currentPts = 0;
    double durationPts = 0;
};