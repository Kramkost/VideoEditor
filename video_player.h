#pragma once
#include <string>
#include <vector>
#include <cmath>
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
    
    float GetProgress() { return (durationSec > 0.0) ? static_cast<float>(currentSec / durationSec) : 0.0f; }
    void Seek(float progress, bool drawVideo = true);
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
    
    double lastQueuedAudioPts = 0.0;
    
    struct CachedFrame {
        AVFrame* frame = nullptr;
        double pts = 0.0;
    };
    std::vector<CachedFrame> frameCache;
    std::vector<AVFrame*> videoFrameQueue;

    double get_audio_clock() {
        if (!audioDevice || audioStreamIndex == -1) return currentSec;
        int queued_bytes = SDL_GetQueuedAudioSize(audioDevice);
        double queued_sec = (double)queued_bytes / (44100.0 * 4.0); // 16-bit stereo = 4 bytes per sample frame
        return lastQueuedAudioPts - queued_sec;
    }
    
    void AddFrameToCache(AVFrame* srcFrame, double pts);
    void ClearFrameCache();
    void ClearVideoQueue();
};