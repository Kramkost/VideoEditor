#pragma once
#include <string>
#include <SDL2/SDL.h>
#include "plugins.h" // Подключаем плагины

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
    
    // Передаем список эффектов в отрисовку
    void UpdateAndDraw(SDL_Renderer* renderer, int viewX, int viewY, int viewW, int viewH, double targetTimeSec, bool drawVideo, float posX, float posY, float scale, float rotation, const std::vector<EffectParams>& effects);
    
    float GetProgress();
    void Seek(float progress);
    void ClearAudio();
    double GetDurationSeconds();
    double GetCurrentSec(); 

    bool isPlaying = false;
    float currentVolume = 1.0f;
    std::string loadedFilepath = ""; 
    bool isImage = false; 
    bool isLoaded = false;           
    bool textureNeedsUpdate = false;

private:
    double durationSec = 0;
    double currentSec = 0;
    
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* videoCodecCtx = nullptr; 
    int videoStreamIndex = -1;
    SwsContext* sws_ctx = nullptr;
    
    int frameW = 0, frameH = 0; // Сохраняем размеры кадра
    AVFrame* pFrame = nullptr;
    AVFrame* pFrameBGR = nullptr; // Оригинал
    AVFrame* pFrameEffects = nullptr; // Измененный кадр с эффектами
    uint8_t* videoBuffer = nullptr;
    uint8_t* effectsBuffer = nullptr;
    SDL_Texture* texture = nullptr;
    
    AVCodecContext* audioCodecCtx = nullptr;
    int audioStreamIndex = -1;
    SwrContext* swrCtx = nullptr;
    AVFrame* aFrame = nullptr;
    SDL_AudioDeviceID audioDevice = 0;
    uint8_t* audioBuffer = nullptr;
    
    AVPacket* pPacket = nullptr;
};