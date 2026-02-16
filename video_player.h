#pragma once
#include <string>
#include <SDL2/SDL.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    bool LoadVideo(const std::string& filepath, SDL_Renderer* renderer);
    void CloseVideo();
    void UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH);
    
    float GetProgress();
    void Seek(float progress); // НОВОЕ: Перемотка

    bool isPlaying = false;    // НОВОЕ: Состояние паузы/плея

private:
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    int videoStreamIndex = -1;
    SwsContext* sws_ctx = nullptr;
    
    AVFrame* pFrame = nullptr;
    AVFrame* pFrameBGR = nullptr;
    AVPacket* pPacket = nullptr;
    uint8_t* buffer = nullptr;
    
    SDL_Texture* texture = nullptr;
    
    bool isLoaded = false;
    double currentPts = 0;
    double durationPts = 0;
};