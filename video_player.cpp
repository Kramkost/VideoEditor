#include "video_player.h"
#include <algorithm>
#include <iostream>

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
    audioStreamIndex = -1;

    // Ищем потоки видео И аудио
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0) {
            videoStreamIndex = i;
        }
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0) {
            audioStreamIndex = i;
        }
    }

    if (videoStreamIndex == -1) return false; // Без видео работать не будем

    // === 1. НАСТРОЙКА ВИДЕО ===
    AVCodecParameters* vCodecParams = formatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* vCodec = avcodec_find_decoder(vCodecParams->codec_id);
    videoCodecCtx = avcodec_alloc_context3(vCodec);
    avcodec_parameters_to_context(videoCodecCtx, vCodecParams);
    avcodec_open2(videoCodecCtx, vCodec, nullptr);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                SDL_TEXTUREACCESS_STREAMING, 
                                videoCodecCtx->width, videoCodecCtx->height);

    sws_ctx = sws_getContext(videoCodecCtx->width, videoCodecCtx->height, videoCodecCtx->pix_fmt,
                             videoCodecCtx->width, videoCodecCtx->height, AV_PIX_FMT_BGRA,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);

    pFrame = av_frame_alloc();
    pFrameBGR = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, videoCodecCtx->width, videoCodecCtx->height, 1);
    videoBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, videoBuffer, AV_PIX_FMT_BGRA, videoCodecCtx->width, videoCodecCtx->height, 1);

    // === 2. НАСТРОЙКА АУДИО ===
    if (audioStreamIndex != -1) {
        AVCodecParameters* aCodecParams = formatCtx->streams[audioStreamIndex]->codecpar;
        const AVCodec* aCodec = avcodec_find_decoder(aCodecParams->codec_id);
        audioCodecCtx = avcodec_alloc_context3(aCodec);
        avcodec_parameters_to_context(audioCodecCtx, aCodecParams);
        avcodec_open2(audioCodecCtx, aCodec, nullptr);

        aFrame = av_frame_alloc();

        // Просим SDL подготовить колонки (Стерео, 44100 Гц, 16 бит)
        SDL_AudioSpec wanted_spec, spec;
        wanted_spec.freq = 44100;
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.channels = 2;
        wanted_spec.silence = 0;
        wanted_spec.samples = 1024;
        wanted_spec.callback = nullptr; // Мы будем использовать очередь, так проще!

        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);

        // Создаем настройку для наших колонок (Стерео, 2 канала)
        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);

        // В новых версиях FFmpeg используется swr_alloc_set_opts2
        swr_alloc_set_opts2(&swrCtx,
                            &out_ch_layout,                       // Вывод: Стерео
                            AV_SAMPLE_FMT_S16,                    // Вывод: 16 бит (для колонок)
                            44100,                                // Вывод: 44100 Гц
                            &aCodecParams->ch_layout,             // Ввод: как в самом видео (НОВЫЙ API)
                            (AVSampleFormat)aCodecParams->format, // Ввод: формат данных видео
                            aCodecParams->sample_rate,            // Ввод: частота видео
                            0, nullptr);
        
        swr_init(swrCtx);
        // Буфер для готового звука (хватит с запасом)
        audioBuffer = (uint8_t*)av_malloc(192000); 

        SDL_PauseAudioDevice(audioDevice, 0); // Включаем колонки (снимаем с паузы)
    }

    pPacket = av_packet_alloc();
    durationPts = (double)formatCtx->streams[videoStreamIndex]->duration;
    timeBase = av_q2d(formatCtx->streams[videoStreamIndex]->time_base);
    isLoaded = true;
    isPlaying = true; 
    return true;
}
void VideoPlayer::UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH, double targetTimeSec) {
    if (!isLoaded) return;

    if (isPlaying) {
        SDL_PauseAudioDevice(audioDevice, 0); 
        bool frameDecoded = false;
        
        // ЧИТАЕМ ПАКЕТЫ, ТОЛЬКО ЕСЛИ ВРЕМЯ КАДРА МЕНЬШЕ ПОЛЗУНКА ТАЙМЛАЙНА
        while (currentPts * timeBase <= targetTimeSec && av_read_frame(formatCtx, pPacket) >= 0) {
            
            if (pPacket->stream_index == videoStreamIndex) {
                avcodec_send_packet(videoCodecCtx, pPacket);
                if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) {
                    currentPts = (double)pFrame->pts;
                    frameDecoded = true; 
                }
            }
            else if (pPacket->stream_index == audioStreamIndex) {
                avcodec_send_packet(audioCodecCtx, pPacket);
                while (avcodec_receive_frame(audioCodecCtx, aFrame) == 0) {
                    int out_samples = swr_convert(swrCtx, &audioBuffer, 192000, 
                                                  (const uint8_t**)aFrame->data, aFrame->nb_samples);
                    int data_size = out_samples * 2 * 2; 
                    
                    int16_t* samples = (int16_t*)audioBuffer;
                    int num_samples = data_size / 2; 
                    for (int i = 0; i < num_samples; i++) {
                        samples[i] = (int16_t)(samples[i] * currentVolume);
                    }
                    SDL_QueueAudio(audioDevice, audioBuffer, data_size);
                }
            }
            av_packet_unref(pPacket);
        }
        
        // РИСУЕМ ТОЛЬКО ПОСЛЕДНИЙ РАСКОДИРОВАННЫЙ КАДР (ОПТИМИЗАЦИЯ!)
        if (frameDecoded) {
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                      pFrame->linesize, 0, videoCodecCtx->height,
                      pFrameBGR->data, pFrameBGR->linesize);

            SDL_UpdateTexture(texture, nullptr, pFrameBGR->data[0], pFrameBGR->linesize[0]);
        }
    } else {
        SDL_PauseAudioDevice(audioDevice, 1);
    }

    // Отрисовка
    float scaleW = (float)windowW / videoCodecCtx->width;
    float scaleH = (float)windowH / videoCodecCtx->height;
    float finalScale = std::min(scaleW, scaleH);
    
    int finalW = (int)(videoCodecCtx->width * finalScale);
    int finalH = (int)(videoCodecCtx->height * finalScale);
    int xOffset = (windowW - finalW) / 2;
    int yOffset = (windowH - finalH) / 2;
    
    SDL_Rect videoRect = {xOffset, yOffset, finalW, finalH};
    SDL_RenderCopy(renderer, texture, nullptr, &videoRect);
}

void VideoPlayer::Seek(float progress) {
    if (!isLoaded || durationPts <= 0) return;
    
    int64_t target_pts = (int64_t)(progress * durationPts);
    av_seek_frame(formatCtx, videoStreamIndex, target_pts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(videoCodecCtx); 
    
    if (audioStreamIndex != -1) {
        avcodec_flush_buffers(audioCodecCtx);
        SDL_ClearQueuedAudio(audioDevice); // Очищаем старый звук из колонок при перемотке!
    }
    
    bool frameDecoded = false;
    while (!frameDecoded && av_read_frame(formatCtx, pPacket) >= 0) {
        if (pPacket->stream_index == videoStreamIndex) {
            avcodec_send_packet(videoCodecCtx, pPacket);
            if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) {
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, videoCodecCtx->height,
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
    
    if (audioDevice) { SDL_CloseAudioDevice(audioDevice); audioDevice = 0; }
    if (swrCtx) { swr_free(&swrCtx); }
    if (audioBuffer) { av_free(audioBuffer); audioBuffer = nullptr; }
    if (aFrame) { av_frame_free(&aFrame); aFrame = nullptr; }
    if (audioCodecCtx) { avcodec_free_context(&audioCodecCtx); audioCodecCtx = nullptr; }
    
    if (videoBuffer) { av_free(videoBuffer); videoBuffer = nullptr; }
    if (pFrame) { av_frame_free(&pFrame); pFrame = nullptr; }
    if (pFrameBGR) { av_frame_free(&pFrameBGR); pFrameBGR = nullptr; }
    if (pPacket) { av_packet_free(&pPacket); pPacket = nullptr; }
    if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
    if (videoCodecCtx) { avcodec_free_context(&videoCodecCtx); videoCodecCtx = nullptr; }
    if (formatCtx) { avformat_close_input(&formatCtx); formatCtx = nullptr; }
    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
}

double VideoPlayer::GetDurationSeconds() {
    if (!isLoaded || durationPts <= 0) return 0.0;
    return durationPts * timeBase;
}

void VideoPlayer::ClearAudio() {
    if (audioDevice) {
        SDL_ClearQueuedAudio(audioDevice);
    }
}