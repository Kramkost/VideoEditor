#include <iostream>
#include <string>
#include <algorithm> 

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

// ПУТЬ К ВИДЕО
const char* VIDEO_FILE = "C:/Videos/FunnyShort1.mp4"; 

// Размеры области просмотра (без таймлайна)
const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
const int TIMELINE_HEIGHT = 50;

int main(int argc, char* argv[]) {
    // 1. ИНИЦИАЛИЗАЦИЯ
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cout << "SDL Error: " << SDL_GetError() << std::endl;
        system("pause"); return -1;
    }

    // 2. FFMPEG
    AVFormatContext* formatCtx = avformat_alloc_context();
    if (avformat_open_input(&formatCtx, VIDEO_FILE, nullptr, nullptr) != 0) {
        std::cout << "Error opening video: " << VIDEO_FILE << std::endl;
        system("pause"); return -1;
    }
    avformat_find_stream_info(formatCtx, nullptr);

    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    AVCodecParameters* codecParams = formatCtx->streams[videoStreamIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    avcodec_open2(codecCtx, codec, nullptr);

    // 3. SDL ОКНО
    // Общая высота = высота просмотра + высота таймлайна
    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        WINDOW_VIEW_W, WINDOW_VIEW_H + TIMELINE_HEIGHT, 
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, 
                                             SDL_TEXTUREACCESS_STREAMING, 
                                             codecCtx->width, codecCtx->height);

    // 4. SWS (BGRA для правильных цветов)
    struct SwsContext* sws_ctx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_BGRA,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    AVFrame* pFrame = av_frame_alloc();
    AVFrame* pFrameBGR = av_frame_alloc();
    AVPacket* pPacket = av_packet_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, codecCtx->width, codecCtx->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pFrameBGR->data, pFrameBGR->linesize, buffer, AV_PIX_FMT_BGRA, codecCtx->width, codecCtx->height, 1);

    // 5. ЦИКЛ
    bool isRunning = true;
    SDL_Event event;

    while (isRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) isRunning = false;
        }

        if (av_read_frame(formatCtx, pPacket) >= 0) {
            if (pPacket->stream_index == videoStreamIndex) {
                avcodec_send_packet(codecCtx, pPacket);
                if (avcodec_receive_frame(codecCtx, pFrame) == 0) {
                    
                    sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                              pFrame->linesize, 0, codecCtx->height,
                              pFrameBGR->data, pFrameBGR->linesize);

                    SDL_UpdateTexture(texture, nullptr, pFrameBGR->data[0], pFrameBGR->linesize[0]);
                    
                    // --- НАЧАЛО РИСОВАНИЯ ---
                    // Заливаем фон черным (для полос по бокам)
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderClear(renderer);
                    
                    // --- МАСШТАБИРОВАНИЕ С СОХРАНЕНИЕМ ПРОПОРЦИЙ ---
                    // 1. Считаем коэффициенты масштаба по ширине и высоте
                    float scaleW = (float)WINDOW_VIEW_W / codecCtx->width;
                    float scaleH = (float)WINDOW_VIEW_H / codecCtx->height;
                    
                    // 2. Выбираем меньший коэффициент, чтобы видео влезло целиком
                    float finalScale = std::min(scaleW, scaleH);
                    
                    // 3. Новые размеры видео
                    int finalW = (int)(codecCtx->width * finalScale);
                    int finalH = (int)(codecCtx->height * finalScale);
                    
                    // 4. Центрируем видео в области просмотра
                    int xOffset = (WINDOW_VIEW_W - finalW) / 2;
                    int yOffset = (WINDOW_VIEW_H - finalH) / 2;
                    
                    SDL_Rect videoRect = {xOffset, yOffset, finalW, finalH};
                    SDL_RenderCopy(renderer, texture, nullptr, &videoRect);

                    // --- ТАЙМЛАЙН (Рисуем поверх черного фона снизу) ---
                    double duration = (double)formatCtx->streams[videoStreamIndex]->duration;
                    double current = (double)pFrame->pts; 
                    float progress = (duration > 0) ? (float)(current / duration) : 0;

                    // Серая подложка таймлайна
                    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                    SDL_Rect timelineBg = {0, WINDOW_VIEW_H, WINDOW_VIEW_W, TIMELINE_HEIGHT};
                    SDL_RenderFillRect(renderer, &timelineBg);

                    // Зеленая полоска прогресса
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    SDL_Rect progressBar = {0, WINDOW_VIEW_H, (int)(WINDOW_VIEW_W * progress), TIMELINE_HEIGHT};
                    SDL_RenderFillRect(renderer, &progressBar);

                    SDL_RenderPresent(renderer);
                    SDL_Delay(1000 / 30); // Простая задержка
                }
            }
            av_packet_unref(pPacket);
        } else {
             // Видео кончилось, выходим (или можно зациклить)
             isRunning = false;
        }
    }

    // Очистка
    av_free(buffer);
    av_frame_free(&pFrame);
    av_frame_free(&pFrameBGR);
    av_packet_free(&pPacket);
    avformat_close_input(&formatCtx);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}