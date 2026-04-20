#include "video_player.h"
#include <algorithm>
#include <iostream>

VideoPlayer::VideoPlayer() {}
VideoPlayer::~VideoPlayer() { CloseVideo(); }

AVPixelFormat FixDeprecatedFormat(AVPixelFormat fmt) {
    switch (fmt) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
        default: return fmt;
    }
}

bool VideoPlayer::LoadVideo(const std::string& filepath, SDL_Renderer* renderer) {
    if (isLoaded && loadedFilepath == filepath) return true;

    CloseVideo(); 
    loadedFilepath = filepath;

    formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, filepath.c_str(), nullptr, nullptr) != 0) return false;
    avformat_find_stream_info(formatCtx, nullptr);

    durationSec = (double)formatCtx->duration / AV_TIME_BASE;
    isImage = false;
    if (durationSec <= 0.1 || formatCtx->duration < 0) {
        isImage = true;
        durationSec = 10.0; 
    }

    videoStreamIndex = -1; audioStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0) videoStreamIndex = i;
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0) audioStreamIndex = i;
    }

    if (videoStreamIndex != -1) {
        AVCodecParameters* vCodecParams = formatCtx->streams[videoStreamIndex]->codecpar;
        const AVCodec* vCodec = avcodec_find_decoder(vCodecParams->codec_id);
        videoCodecCtx = avcodec_alloc_context3(vCodec);
        avcodec_parameters_to_context(videoCodecCtx, vCodecParams);

        if (!isImage) {
            videoCodecCtx->thread_count = 0; 
            videoCodecCtx->thread_type = FF_THREAD_FRAME;
        }

        avcodec_open2(videoCodecCtx, vCodec, nullptr);

        pFrame = av_frame_alloc(); 
        pFrameBGR = av_frame_alloc();
        pFrameEffects = av_frame_alloc();

        if (isImage) {
            AVPacket* tempPkt = av_packet_alloc();
            bool decoded = false;
            
            while (!decoded && av_read_frame(formatCtx, tempPkt) >= 0) {
                if (tempPkt->stream_index == videoStreamIndex) {
                    avcodec_send_packet(videoCodecCtx, tempPkt);
                    if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) decoded = true;
                }
                av_packet_unref(tempPkt);
            }
            av_packet_free(&tempPkt);

            if (!decoded) {
                avcodec_send_packet(videoCodecCtx, nullptr); 
                if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) decoded = true;
            }

            frameW = pFrame->width > 0 ? pFrame->width : videoCodecCtx->width;
            frameH = pFrame->height > 0 ? pFrame->height : videoCodecCtx->height;

            if (!decoded || frameW <= 0 || frameH <= 0) {
                CloseVideo(); 
                return false;
            }

            AVPixelFormat actualFormat = FixDeprecatedFormat((AVPixelFormat)pFrame->format);
            if (actualFormat == AV_PIX_FMT_NONE) actualFormat = FixDeprecatedFormat(videoCodecCtx->pix_fmt);

            sws_ctx = sws_getContext(frameW, frameH, actualFormat, frameW, frameH, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frameW, frameH);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

            int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, frameW, frameH, 1);
            videoBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
            effectsBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
            
            av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, videoBuffer, AV_PIX_FMT_BGRA, frameW, frameH, 1);
            av_image_fill_arrays(pFrameEffects->data, pFrameEffects->linesize, effectsBuffer, AV_PIX_FMT_BGRA, frameW, frameH, 1);

            if (sws_ctx && pFrame->data[0]) {
                sws_scale(sws_ctx, pFrame->data, pFrame->linesize, 0, frameH, pFrameBGR->data, pFrameBGR->linesize);
                textureNeedsUpdate = true; // Триггерим отрисовку для картинок
            }
        } 
        else {
            frameW = videoCodecCtx->width;
            frameH = videoCodecCtx->height;
            AVPixelFormat fmt = FixDeprecatedFormat(videoCodecCtx->pix_fmt);
            if (fmt == AV_PIX_FMT_NONE) fmt = AV_PIX_FMT_YUV420P; 

            sws_ctx = sws_getContext(frameW, frameH, fmt, frameW, frameH, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, frameW, frameH);
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

            int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, frameW, frameH, 1);
            videoBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
            effectsBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
            
            av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, videoBuffer, AV_PIX_FMT_BGRA, frameW, frameH, 1);
            av_image_fill_arrays(pFrameEffects->data, pFrameEffects->linesize, effectsBuffer, AV_PIX_FMT_BGRA, frameW, frameH, 1);
        }
    }

    if (audioStreamIndex != -1) {
        AVCodecParameters* aCodecParams = formatCtx->streams[audioStreamIndex]->codecpar;
        const AVCodec* aCodec = avcodec_find_decoder(aCodecParams->codec_id);
        audioCodecCtx = avcodec_alloc_context3(aCodec);
        avcodec_parameters_to_context(audioCodecCtx, aCodecParams);
        avcodec_open2(audioCodecCtx, aCodec, nullptr);

        aFrame = av_frame_alloc();
        SDL_AudioSpec wanted_spec, spec;
        wanted_spec.freq = 44100; wanted_spec.format = AUDIO_S16SYS; wanted_spec.channels = 2; wanted_spec.silence = 0; wanted_spec.samples = 1024; wanted_spec.callback = nullptr; 

        audioDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &spec, 0);

        AVChannelLayout out_ch_layout; av_channel_layout_default(&out_ch_layout, 2);
        swr_alloc_set_opts2(&swrCtx, &out_ch_layout, AV_SAMPLE_FMT_S16, 44100, &aCodecParams->ch_layout, (AVSampleFormat)aCodecParams->format, aCodecParams->sample_rate, 0, nullptr);
        swr_init(swrCtx);

        audioBuffer = (uint8_t*)av_malloc(192000); 
        SDL_PauseAudioDevice(audioDevice, 0); 
    }

    pPacket = av_packet_alloc();
    isLoaded = true; isPlaying = true; 
    return true;
}

void VideoPlayer::UpdateAndDraw(SDL_Renderer* renderer, int viewX, int viewY, int viewW, int viewH, double targetTimeSec, bool drawVideo, float posX, float posY, float scale, float rotation, const std::vector<EffectParams>& effects) {
    if (!isLoaded) return;

    bool frameDecoded = false; 

    if (isPlaying) {
        if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0); 
        
        if (!isImage) {
            // ФИКС 1: ЗАЩИТА ОТ ПУСТОГО ВИДЕО (Если это просто аудиофайл)
            if (videoCodecCtx) {
                if (targetTimeSec - currentSec > 0.15) videoCodecCtx->skip_frame = AVDISCARD_NONREF; 
                else videoCodecCtx->skip_frame = AVDISCARD_DEFAULT;
            }

            int loopProtection = 0; 
            while (currentSec <= targetTimeSec && loopProtection < 10 && av_read_frame(formatCtx, pPacket) >= 0) {
                loopProtection++;
                
                // Обработка ВИДЕО
                if (pPacket->stream_index == videoStreamIndex && videoCodecCtx) {
                    avcodec_send_packet(videoCodecCtx, pPacket);
                    if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) {
                        currentSec = pFrame->pts * av_q2d(formatCtx->streams[videoStreamIndex]->time_base);
                        frameDecoded = true; 
                    }
                }
                // Обработка АУДИО
                else if (pPacket->stream_index == audioStreamIndex && audioDevice) {
                    double audioPtsSec = pPacket->pts * av_q2d(formatCtx->streams[audioStreamIndex]->time_base);
                    
                    // ФИКС 2: ЕСЛИ ЭТО ТОЛЬКО АУДИО (НЕТ ВИДЕО), ВРЕМЯ ДИКТУЕТ ЗВУК!
                    if (videoStreamIndex == -1) {
                        currentSec = audioPtsSec; 
                    }

                    avcodec_send_packet(audioCodecCtx, pPacket);
                    while (avcodec_receive_frame(audioCodecCtx, aFrame) == 0) {
                        // ФИКС 3: Защита swrCtx и проверка на отрицательный результат
                        if (swrCtx && audioPtsSec >= targetTimeSec - 0.15) {
                            int out_samples = swr_convert(swrCtx, &audioBuffer, 48000, (const uint8_t**)aFrame->data, aFrame->nb_samples);
                            if (out_samples > 0) {
                                int data_size = out_samples * 2 * 2; 
                                int16_t* samples = (int16_t*)audioBuffer;
                                for (int i = 0; i < data_size / 2; i++) samples[i] = (int16_t)(samples[i] * currentVolume);
                                SDL_QueueAudio(audioDevice, audioBuffer, data_size);
                            }
                        }
                    }
                }
                av_packet_unref(pPacket);
            }
        }
    } else {
        if (audioDevice) SDL_PauseAudioDevice(audioDevice, 1);
    }

    // TODO: [МАГИЯ ЭФФЕКТОВ НА ПИКСЕЛЯХ]
    // ... остальной твой код без изменений

    // TODO: [МАГИЯ ЭФФЕКТОВ НА ПИКСЕЛЯХ]
    if (drawVideo && (frameDecoded || textureNeedsUpdate) && videoStreamIndex != -1) {
        if (!isImage && pFrame && pFrame->data[0] != nullptr && sws_ctx) {
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0, frameH, pFrameBGR->data, pFrameBGR->linesize);
        }
        
        // 1. Берем чистую копию кадра
        memcpy(pFrameEffects->data[0], pFrameBGR->data[0], pFrameBGR->linesize[0] * frameH);
        
        // 2. Накладываем все эффекты через наш PluginManager (С ПЕРЕДАЧЕЙ currentSec)
        PluginManager::ApplyPlugins(pFrameEffects->data[0], frameW, frameH, pFrameEffects->linesize[0], effects, (float)currentSec);
        
        // 3. Отправляем на видеокарту
        SDL_UpdateTexture(texture, nullptr, pFrameEffects->data[0], pFrameEffects->linesize[0]);
        textureNeedsUpdate = false; 
    }
    // ОТРИСОВКА НА ЭКРАН
    if (drawVideo && videoStreamIndex != -1 && texture) {
        float scaleW = (float)viewW / frameW;
        float scaleH = (float)viewH / frameH;
        float baseScale = std::min(scaleW, scaleH);
        float finalScale = baseScale * scale; 
        
        int finalW = (int)(frameW * finalScale);
        int finalH = (int)(frameH * finalScale);
        
        if (finalW > 0 && finalH > 0) {
            int xOffset = viewX + (viewW - finalW) / 2 + (int)posX;
            int yOffset = viewY + (viewH - finalH) / 2 + (int)posY;
            
            SDL_Rect videoRect = {xOffset, yOffset, finalW, finalH};
            SDL_RenderCopyEx(renderer, texture, nullptr, &videoRect, (double)rotation, nullptr, SDL_FLIP_NONE);
        }
    }
}

void VideoPlayer::Seek(float progress) {
    if (!isLoaded || isImage) return; 
    
    int64_t target_pts_av = (int64_t)(progress * formatCtx->duration);
    av_seek_frame(formatCtx, -1, target_pts_av, AVSEEK_FLAG_BACKWARD);
    
    if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx); 
    if (audioCodecCtx) { avcodec_flush_buffers(audioCodecCtx); if(audioDevice) SDL_ClearQueuedAudio(audioDevice); }
    
    if (videoStreamIndex != -1) {
        bool frameDecoded = false;
        while (!frameDecoded && av_read_frame(formatCtx, pPacket) >= 0) {
            if (pPacket->stream_index == videoStreamIndex) {
                avcodec_send_packet(videoCodecCtx, pPacket);
                if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) {
                    currentSec = pFrame->pts * av_q2d(formatCtx->streams[videoStreamIndex]->time_base);
                    frameDecoded = true;
                }
            }
            av_packet_unref(pPacket);
        }
    } else {
        currentSec = target_pts_av / (double)AV_TIME_BASE; 
    }
    textureNeedsUpdate = true; 
}

double VideoPlayer::GetDurationSeconds() { return durationSec; }
double VideoPlayer::GetCurrentSec() { return currentSec; }
void VideoPlayer::ClearAudio() { if (audioDevice) SDL_ClearQueuedAudio(audioDevice); }

void VideoPlayer::CloseVideo() {
    if (!isLoaded) return;
    isLoaded = false; isPlaying = false;
    if (audioDevice) { SDL_CloseAudioDevice(audioDevice); audioDevice = 0; }
    if (swrCtx) { swr_free(&swrCtx); }
    if (audioBuffer) { av_free(audioBuffer); audioBuffer = nullptr; }
    if (effectsBuffer) { av_free(effectsBuffer); effectsBuffer = nullptr; }
    if (aFrame) { av_frame_free(&aFrame); aFrame = nullptr; }
    if (audioCodecCtx) { avcodec_free_context(&audioCodecCtx); audioCodecCtx = nullptr; }
    if (videoBuffer) { av_free(videoBuffer); videoBuffer = nullptr; }
    if (pFrame) { av_frame_free(&pFrame); pFrame = nullptr; }
    if (pFrameBGR) { av_frame_free(&pFrameBGR); pFrameBGR = nullptr; }
    if (pFrameEffects) { av_frame_free(&pFrameEffects); pFrameEffects = nullptr; }
    if (pPacket) { av_packet_free(&pPacket); pPacket = nullptr; }
    if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
    if (videoCodecCtx) { avcodec_free_context(&videoCodecCtx); videoCodecCtx = nullptr; }
    if (formatCtx) { avformat_close_input(&formatCtx); formatCtx = nullptr; }
    if (texture) { SDL_DestroyTexture(texture); texture = nullptr; }
}
