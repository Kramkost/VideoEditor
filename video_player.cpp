#include "video_player.h"
#include <algorithm>
#include <iostream>

VideoPlayer::VideoPlayer() {}

VideoPlayer::~VideoPlayer() { CloseVideo(); }

bool VideoPlayer::LoadVideo(const std::string& filepath, SDL_Renderer* renderer) {
    // ОПТИМИЗАЦИЯ: Если файл уже загружен - ничего не делаем!
    if (isLoaded && loadedFilepath == filepath) return true;

    CloseVideo(); 
    loadedFilepath = filepath;

    formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, filepath.c_str(), nullptr, nullptr) != 0) return false;
    avformat_find_stream_info(formatCtx, nullptr);

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
        avcodec_open2(videoCodecCtx, vCodec, nullptr);

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, videoCodecCtx->width, videoCodecCtx->height);
        sws_ctx = sws_getContext(videoCodecCtx->width, videoCodecCtx->height, videoCodecCtx->pix_fmt, videoCodecCtx->width, videoCodecCtx->height, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);

        pFrame = av_frame_alloc(); pFrameBGR = av_frame_alloc();
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, videoCodecCtx->width, videoCodecCtx->height, 1);
        videoBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, videoBuffer, AV_PIX_FMT_BGRA, videoCodecCtx->width, videoCodecCtx->height, 1);
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
    durationSec = (double)formatCtx->duration / AV_TIME_BASE;
    isLoaded = true; isPlaying = true; 
    return true;
}

void VideoPlayer::UpdateAndDraw(SDL_Renderer* renderer, int windowW, int windowH, double targetTimeSec, bool drawVideo) {
    if (!isLoaded) return;

    if (isPlaying) {
        SDL_PauseAudioDevice(audioDevice, 0); 
        bool frameDecoded = false;
        
        while (currentSec <= targetTimeSec && av_read_frame(formatCtx, pPacket) >= 0) {
            if (pPacket->stream_index == videoStreamIndex) {
                avcodec_send_packet(videoCodecCtx, pPacket);
                if (avcodec_receive_frame(videoCodecCtx, pFrame) == 0) {
                    currentSec = pFrame->pts * av_q2d(formatCtx->streams[videoStreamIndex]->time_base);
                    frameDecoded = true; 
                }
            }
            else if (pPacket->stream_index == audioStreamIndex) {
                double audioPtsSec = pPacket->pts * av_q2d(formatCtx->streams[audioStreamIndex]->time_base);
                avcodec_send_packet(audioCodecCtx, pPacket);
                while (avcodec_receive_frame(audioCodecCtx, aFrame) == 0) {
                    // TODO: [ВАЖНО] ФИКС БАГА! Защита от "наслоения" старого звука
                    if (audioPtsSec >= targetTimeSec - 0.15) {
                        int out_samples = swr_convert(swrCtx, &audioBuffer, 192000, (const uint8_t**)aFrame->data, aFrame->nb_samples);
                        int data_size = out_samples * 2 * 2; 
                        
                        int16_t* samples = (int16_t*)audioBuffer;
                        int num_samples = data_size / 2; 
                        for (int i = 0; i < num_samples; i++) samples[i] = (int16_t)(samples[i] * currentVolume);
                        SDL_QueueAudio(audioDevice, audioBuffer, data_size);
                    }
                }
            }
            av_packet_unref(pPacket);
        }
        
        // Масштабируем картинку только если мы разрешили рисовать видео!
        if (drawVideo && frameDecoded && videoStreamIndex != -1) {
            sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data, pFrame->linesize, 0, videoCodecCtx->height, pFrameBGR->data, pFrameBGR->linesize);
            SDL_UpdateTexture(texture, nullptr, pFrameBGR->data[0], pFrameBGR->linesize[0]);
        }
    } else {
        SDL_PauseAudioDevice(audioDevice, 1);
    }

    if (drawVideo && videoStreamIndex != -1) {
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
}

void VideoPlayer::Seek(float progress) {
    if (!isLoaded || durationSec <= 0) return;
    
    int64_t target_pts_av = (int64_t)(progress * formatCtx->duration);
    av_seek_frame(formatCtx, -1, target_pts_av, AVSEEK_FLAG_BACKWARD);
    
    if (videoCodecCtx) avcodec_flush_buffers(videoCodecCtx); 
    if (audioCodecCtx) { avcodec_flush_buffers(audioCodecCtx); SDL_ClearQueuedAudio(audioDevice); }
    
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
        currentSec = target_pts_av / (double)AV_TIME_BASE; // Для аудио-файлов
    }
}

double VideoPlayer::GetDurationSeconds() { return durationSec; }
void VideoPlayer::ClearAudio() { if (audioDevice) SDL_ClearQueuedAudio(audioDevice); }
void VideoPlayer::CloseVideo() {
    if (!isLoaded) return;
    isLoaded = false; isPlaying = false;
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
float VideoPlayer::GetProgress() { return (float)(currentSec / durationSec); }