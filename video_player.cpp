#include "video_player.h"
#include <algorithm>

VideoPlayer::VideoPlayer() {}

VideoPlayer::~VideoPlayer() {
    CloseVideo();
}

bool VideoPlayer::LoadVideo(const std::string& filepath, SDL_Renderer* renderer) {
    CloseVideo(); 

    formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, filepath.c_str(), nullptr, nullptr) != 0) return false;
    avformat_find_stream_info(formatCtx, nullptr);

    videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1) return false;

    AVCodecParameters* codecParams = formatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    avcodec_open2(codecCtx, codec, nullptr);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                codecCtx->width, codecCtx->height);

    sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
                             codecCtx->width, codecCtx->height, AV_PIX_FMT_BGRA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);

    pFrame = av_frame_alloc();
    pFrameBGR = av_frame_alloc();
    pPacket = av_packet_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, codecCtx->width, codecCtx->height, 1);
    buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, buffer, AV_PIX_FMT_BGRA, codecCtx->width, codecCtx->height, 1);

    durationPts = (double)formatCtx->streams[videoStreamIndex]->duration;
    isLoaded = true;
    isPlaying = true; // Сразу запускаем видео после загрузки
    return true;
}

void VideoPlayer::UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH) {
    if (!isLoaded) return;

    // Читаем кадры ТОЛЬКО если видео играет
    if (isPlaying) {
        bool frameDecoded = false;
        // КРУТОЙ ФИКС: Читаем пакеты, ПОКА не получим видеокадр
        while (!frameDecoded && av_read_frame(formatCtx, pPacket) >= 0) {
            if (pPacket->stream_index == videoStreamIndex) {
                avcodec_send_packet(codecCtx, pPacket);
                if (avcodec_receive_frame(codecCtx, pFrame) == 0) {
                    sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                              pFrame->linesize, 0, codecCtx->height,
                              pFrameBGR->data, pFrameBGR->linesize);

                    SDL_UpdateTexture(texture, nullptr, pFrameBGR->data[0], pFrameBGR->linesize[0]);
                    currentPts = (double)pFrame->pts;
                    frameDecoded = true; // Нашли кадр, выходим из цикла!
                }
            }
            av_packet_unref(pPacket);
        }
        
        if (!frameDecoded) {
            isPlaying = false; // Видео закончилось
        }
    }

    // Отрисовка работает всегда (чтобы кадр висел на паузе)
    float scaleW = (float)windowW / codecCtx->width;
    float scaleH = (float)windowH / codecCtx->height;
    float finalScale = std::min(scaleW, scaleH);
    
    int finalW = (int)(codecCtx->width * finalScale);
    int finalH = (int)(codecCtx->height * finalScale);
    int xOffset = (windowW - finalW) / 2;
    int yOffset = (windowH - finalH) / 2;
    
    SDL_Rect videoRect = {xOffset, yOffset, finalW, finalH};
    SDL_RenderCopy(renderer, texture, nullptr, &videoRect);
}

void VideoPlayer::Seek(float progress) {
    if (!isLoaded || durationPts <= 0) return;
    
    // Вычисляем нужный тайминг и просим FFmpeg прыгнуть туда
    int64_t target_pts = (int64_t)(progress * durationPts);
    av_seek_frame(formatCtx, videoStreamIndex, target_pts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCtx); // Сбрасываем старые кадры из памяти кодека
    
    // Сразу декодируем один кадр, чтобы обновить картинку на экране во время паузы
    bool frameDecoded = false;
    while (!frameDecoded && av_read_frame(formatCtx, pPacket) >= 0) {
        if (pPacket->stream_index == videoStreamIndex) {
            avcodec_send_packet(codecCtx, pPacket);
            if (avcodec_receive_frame(codecCtx, pFrame) == 0) {
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, codecCtx->height,
                          pFrameBGR->data, pFrameBGR->linesize);
                SDL_UpdateTexture(texture, nullptr, pFrameBGR->data[0], pFrameBGR->linesize[0]);
                currentPts = (double)pFrame->pts;
                frameDecoded = true;
            }
        }
        av_packet_unref(pPacket);
    }
}

float VideoPlayer::GetProgress() {
    if (!isLoaded || durationPts <= 0) return 0.0f;
    return (float)(currentPts / durationPts);
}

void VideoPlayer::CloseVideo() {
    if (!isLoaded) return;
    isLoaded = false;
    isPlaying = false;
    
    if (buffer) { av_free(buffer); buffer = nullptr; }
    if (pFrame) { av_frame_free(&pFrame); pFrame = nullptr; }
    if (pFrameBGR) { av_frame_free(&pFrameBGR); pFrameBGR = nullptr; }
    if (pPacket) { av_packet_free(&pPacket); pPacket = nullptr; }
    if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
    if (codecCtx) { avcodec_free_context(&codecCtx); codecCtx = nullptr; }
    if (formatCtx) { avformat_close_input(&formatCtx); formatCtx = nullptr; }
    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
}