#include <iostream>
#include <string>
#include <vector>
#include <cmath> 
#include <algorithm>
#include <filesystem>
#include <fstream>

#include "export_ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"         
#include "imgui_impl_sdlrenderer2.h" 
#include "project_manager.h"         

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "ui.h"
#include "video_player.h"
#include "logger.h"
#include "math_utils.h"
#include <unordered_map>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libswresample/swresample.h>
    #include <libavutil/channel_layout.h>
}

double GetFileDuration(const std::string& filepath) {
    if (filepath.empty()) return 0.0;
    AVFormatContext* tempCtx = avformat_alloc_context();
    if (avformat_open_input(&tempCtx, filepath.c_str(), nullptr, nullptr) != 0) {
        return 0.0;
    }
    avformat_find_stream_info(tempCtx, nullptr);
    double duration = 0.0;
    if (tempCtx->duration > 0) {
        duration = (double)tempCtx->duration / AV_TIME_BASE;
    }
    avformat_close_input(&tempCtx);
    return duration;
}

TTF_Font* g_Font = nullptr;

struct WavHeader {
    char chunkId[4] = {'R', 'I', 'F', 'F'};
    uint32_t chunkSize;
    char format[4] = {'W', 'A', 'V', 'E'};
    char subchunk1Id[4] = {'f', 'm', 't', ' '};
    uint32_t subchunk1Size = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t numChannels = 2; // Stereo
    uint32_t sampleRate = 44100;
    uint32_t byteRate = 44100 * 2 * 2;
    uint16_t blockAlign = 2 * 2;
    uint16_t bitsPerSample = 16;
    char subchunk2Id[4] = {'d', 'a', 't', 'a'};
    uint32_t subchunk2Size;
};

bool RenderProjectAudio(const ProjectData& project, double projectDurationSec, const std::string& wavPath) {
    int sampleRate = 44100;
    int numChannels = 2;
    int totalSamples = static_cast<int>(projectDurationSec * sampleRate);
    if (totalSamples <= 0) return false;

    std::vector<float> mixBuffer(totalSamples * numChannels, 0.0f);

    for (const auto& clip : project.clips) {
        if (clip.isText || clip.isNullObject || clip.filepath.empty() || clip.volume <= 0.001f) {
            continue;
        }

        AVFormatContext* fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, clip.filepath.c_str(), nullptr, nullptr) != 0) {
            continue;
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            avformat_close_input(&fmtCtx);
            continue;
        }

        int audioIdx = -1;
        for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i) {
            if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audioIdx = i;
                break;
            }
        }

        if (audioIdx == -1) {
            avformat_close_input(&fmtCtx);
            continue;
        }

        AVCodecParameters* codecParams = fmtCtx->streams[audioIdx]->codecpar;
        const AVCodec* decoder = avcodec_find_decoder(codecParams->codec_id);
        if (!decoder) {
            avformat_close_input(&fmtCtx);
            continue;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
        if (!codecCtx) {
            avformat_close_input(&fmtCtx);
            continue;
        }

        avcodec_parameters_to_context(codecCtx, codecParams);
        if (avcodec_open2(codecCtx, decoder, nullptr) < 0) {
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            continue;
        }

        double fileDuration = 0.0;
        if (fmtCtx->duration > 0) {
            fileDuration = (double)fmtCtx->duration / AV_TIME_BASE;
        }

        float timelineStartSec = clip.timelineStart * projectDurationSec;
        float timelineEndSec = clip.timelineEnd * projectDurationSec;
        float timelineDuration = timelineEndSec - timelineStartSec;

        float mediaStartSec = clip.mediaStart * fileDuration;
        float mediaEndSec = clip.mediaEnd * fileDuration;
        float mediaDuration = mediaEndSec - mediaStartSec;

        if (timelineDuration <= 0.01f || mediaDuration <= 0.01f) {
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            continue;
        }

        double speed = mediaDuration / timelineDuration;

        SwrContext* swr = nullptr;
        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);

        int targetSampleRate = 44100;
        int sourceSampleRate = static_cast<int>(codecCtx->sample_rate * speed);

        swr_alloc_set_opts2(&swr,
                            &out_ch_layout, AV_SAMPLE_FMT_FLT, targetSampleRate,
                            &codecParams->ch_layout, codecCtx->sample_fmt, codecCtx->sample_rate,
                            0, nullptr);
        if (!swr || swr_init(swr) < 0) {
            if (swr) swr_free(&swr);
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            continue;
        }

        int64_t seek_target = static_cast<int64_t>(mediaStartSec * AV_TIME_BASE);
        av_seek_frame(fmtCtx, -1, seek_target, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(codecCtx);

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();

        int max_out_samples = 4096;
        float* resampleBuf = nullptr;
        av_samples_alloc((uint8_t**)&resampleBuf, nullptr, 2, max_out_samples, AV_SAMPLE_FMT_FLT, 0);

        int timelineStartSample = static_cast<int>(timelineStartSec * sampleRate);
        int timelineEndSample = static_cast<int>(timelineEndSec * sampleRate);
        int currentOutSample = timelineStartSample;

        bool done = false;
        while (!done && av_read_frame(fmtCtx, packet) >= 0) {
            if (packet->stream_index == audioIdx) {
                double ptsSec = packet->pts * av_q2d(fmtCtx->streams[audioIdx]->time_base);
                if (ptsSec > mediaEndSec) {
                    av_packet_unref(packet);
                    break;
                }

                if (avcodec_send_packet(codecCtx, packet) == 0) {
                    while (avcodec_receive_frame(codecCtx, frame) == 0) {
                        double framePts = frame->pts * av_q2d(fmtCtx->streams[audioIdx]->time_base);
                        if (framePts < mediaStartSec - 0.2) {
                            continue;
                        }

                        int out_count = swr_get_out_samples(swr, frame->nb_samples);
                        if (out_count > max_out_samples) {
                            max_out_samples = out_count + 1024;
                            av_freep(&resampleBuf);
                            av_samples_alloc((uint8_t**)&resampleBuf, nullptr, 2, max_out_samples, AV_SAMPLE_FMT_FLT, 0);
                        }

                        int converted = swr_convert(swr, (uint8_t**)&resampleBuf, max_out_samples,
                                                    (const uint8_t**)frame->data, frame->nb_samples);
                        if (converted > 0) {
                            for (int i = 0; i < converted; ++i) {
                                int outIdx = currentOutSample + i;
                                if (outIdx >= timelineEndSample || outIdx >= totalSamples) {
                                    done = true;
                                    break;
                                }
                                mixBuffer[outIdx * 2]     += resampleBuf[i * 2] * clip.volume;
                                mixBuffer[outIdx * 2 + 1] += resampleBuf[i * 2 + 1] * clip.volume;
                            }
                            currentOutSample += converted;
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }

        int converted = swr_convert(swr, (uint8_t**)&resampleBuf, max_out_samples, nullptr, 0);
        if (converted > 0) {
            for (int i = 0; i < converted; ++i) {
                int outIdx = currentOutSample + i;
                if (outIdx >= timelineEndSample || outIdx >= totalSamples) break;
                mixBuffer[outIdx * 2]     += resampleBuf[i * 2] * clip.volume;
                mixBuffer[outIdx * 2 + 1] += resampleBuf[i * 2 + 1] * clip.volume;
            }
        }

        av_freep(&resampleBuf);
        av_frame_free(&frame);
        av_packet_free(&packet);
        swr_free(&swr);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&fmtCtx);
    }

    std::vector<int16_t> pcmData(totalSamples * numChannels);
    for (int i = 0; i < totalSamples * numChannels; ++i) {
        float val = mixBuffer[i];
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        pcmData[i] = static_cast<int16_t>(val * 32767.0f);
    }

    FILE* f = fopen(wavPath.c_str(), "wb");
    if (!f) return false;

    WavHeader header;
    header.subchunk2Size = totalSamples * numChannels * 2;
    header.chunkSize = 36 + header.subchunk2Size;

    fwrite(&header, 1, sizeof(header), f);
    fwrite(pcmData.data(), 1, pcmData.size() * 2, f);
    fclose(f);

    return true;
}

#define AUTOSAVE_INTERVAL_SEC 60.0f

const int WINDOW_VIEW_W = 1280;
const int WINDOW_VIEW_H = 720;
const int EXTRA_UI_HEIGHT = 250; 

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
        LOG_ERROR("Failed to initialize SDL.");
        return -1; 
    }
    if (TTF_Init() < 0) {
        LOG_ERROR("Failed to initialize SDL_ttf: %s", TTF_GetError());
    }

    std::vector<std::string> fontPaths = {
        "/usr/share/fonts/google-carlito-fonts/Carlito-Regular.ttf",
        "/usr/share/fonts/google-droid-sans-fonts/DroidSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "arial.ttf"
    };
    for (const auto& path : fontPaths) {
        g_Font = TTF_OpenFont(path.c_str(), 64);
        if (g_Font) {
            LOG_DEBUG("Loaded font: %s", path.c_str());
            break;
        }
    }
    if (!g_Font) {
        LOG_ERROR("Failed to load any font!");
    }

    SDL_Window* window = SDL_CreateWindow("Titan Video Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    UIManager ui;
    ui.Init(window, renderer);
    ExportUI exportMenu;        
    bool showExportMenu = false;

    PluginManager::InitAndScanPlugins();
    ProjectManager::Init(); 

    ProjectData currentProject;  
    bool isProjectOpen = false;  
    std::unordered_map<std::string, double> mediaDurationCache;
    
    std::vector<std::unique_ptr<VideoPlayer>> players;
    std::vector<int> lastActiveClipPerTrack;
    int selectedClipIndex = -1;

    bool isPlaying = false;
    bool isRunning = true;
    SDL_Event event;

    Uint64 lastTime = SDL_GetPerformanceCounter();
    double perfFrequency = static_cast<double>(SDL_GetPerformanceFrequency()); 
    float currentProgress = 0.0f; 

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    int lastMouseX = 0, lastMouseY = 0;

    bool isExporting = false;
    int exportFrameCurrent = 0;
    int exportFrameTotal = 0;
    FILE* ffmpegPipe = nullptr;
    std::vector<uint8_t> exportPixelBuffer;
    SDL_Texture* exportTargetTexture = nullptr;

    float autoSaveTimer = 0.0f;
    std::string autoSavePath = "project_autosave.titansave";

    while (isRunning) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(currentTime - lastTime) / static_cast<float>(perfFrequency);
        lastTime = currentTime;

        if (dt > 0.1f) dt = 0.1f;

        while (SDL_PollEvent(&event)) {
            ui.ProcessEvent(&event); 
            if (event.type == SDL_QUIT) isRunning = false;
            
            if (event.type == SDL_DROPFILE) {
                std::string droppedFile = event.drop.file;
                SDL_free(event.drop.file); 
                
                if (isProjectOpen) {
                    if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), droppedFile) == currentProject.mediaFiles.end()) {
                        currentProject.mediaFiles.push_back(droppedFile);
                        LOG_DEBUG("Imported media: %s", droppedFile.c_str());
                    }
                }
            }
            
            if (event.type == SDL_KEYDOWN && isProjectOpen && !isExporting && !ImGui::GetIO().WantCaptureKeyboard) {
                if (event.key.keysym.sym == SDLK_SPACE) isPlaying = !isPlaying; 
                if (event.key.keysym.sym == SDLK_RIGHT) currentProgress = std::clamp(currentProgress + 0.05f, 0.0f, 1.0f);
                if (event.key.keysym.sym == SDLK_LEFT)  currentProgress = std::clamp(currentProgress - 0.05f, 0.0f, 1.0f);
                
                if (event.key.keysym.sym == SDLK_DELETE || event.key.keysym.sym == SDLK_BACKSPACE) {
                    if (selectedClipIndex >= 0 && selectedClipIndex < currentProject.clips.size()) {
                        currentProject.clips.erase(currentProject.clips.begin() + selectedClipIndex);
                        selectedClipIndex = -1;
                    }
                }
                
                if (event.key.keysym.sym == SDLK_s && (SDL_GetModState() & KMOD_CTRL)) {
                    if (currentProject.saveFilepath.empty()) ProjectManager::SaveProjectAs(currentProject);
                    else ProjectManager::SaveProject(currentProject);
                }
            }
        }

        if (isProjectOpen && !isExporting) {
            autoSaveTimer += dt;
            if (autoSaveTimer >= AUTOSAVE_INTERVAL_SEC) {
                std::string originalPath = currentProject.saveFilepath;
                currentProject.saveFilepath = autoSavePath;
                ProjectManager::SaveProject(currentProject);
                currentProject.saveFilepath = originalPath;
                autoSaveTimer = 0.0f;
            }
        }

        if (!isProjectOpen) {
            SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255); 
            SDL_RenderClear(renderer);

            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            if (ProjectManager::DrawStartScreen(WINDOW_VIEW_W, WINDOW_VIEW_H + EXTRA_UI_HEIGHT, currentProject)) {
                isProjectOpen = true; 
                players.clear();
                for (int i = 0; i < currentProject.tracks.size(); i++) {
                    players.push_back(std::make_unique<VideoPlayer>());
                    lastActiveClipPerTrack.push_back(-1);
                }
            }

            ImGui::Render();
            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        } 
        else {
            int leftPanelW = (ui.currentWorkspace == UIManager::WORKSPACE_EFFECTS) ? 350 : 220;
            int rightPanelW = (ui.currentWorkspace == UIManager::WORKSPACE_EFFECTS) ? 0 : 300;
            int viewW = WINDOW_VIEW_W - leftPanelW - rightPanelW;
            
            double maxDurationSec = 1.0;
            for (const auto& clip : currentProject.clips) {
                if (clip.isText || clip.isNullObject || clip.filepath.empty()) continue;
                double fileDuration = 0.0;
                auto it = mediaDurationCache.find(clip.filepath);
                if (it != mediaDurationCache.end()) {
                    fileDuration = it->second;
                } else {
                    fileDuration = GetFileDuration(clip.filepath);
                    mediaDurationCache[clip.filepath] = fileDuration;
                }
                
                if (fileDuration > 0.0) {
                    float timelineLen = clip.timelineEnd - clip.timelineStart;
                    float mediaLen = clip.mediaEnd - clip.mediaStart;
                    if (timelineLen > 0.001f && mediaLen > 0.001f) {
                        double durationProposed = fileDuration * (mediaLen / timelineLen);
                        if (durationProposed > maxDurationSec) {
                            maxDurationSec = durationProposed;
                        }
                    }
                }
            }
            
            if (exportMenu.startRender) {
                exportMenu.startRender = false;
                isExporting = true; isPlaying = false; exportFrameCurrent = 0;
                
                float maxTimelineEnd = 0.0f;
                for (const auto& clip : currentProject.clips) {
                    if (clip.timelineEnd > maxTimelineEnd) maxTimelineEnd = clip.timelineEnd;
                }
                exportFrameTotal = static_cast<int>(maxTimelineEnd * maxDurationSec * exportMenu.fps);
                if (exportFrameTotal == 0) isExporting = false; 
                
                if (isExporting) {
                    RenderProjectAudio(currentProject, maxDurationSec, "temp_audio.wav");

                    exportTargetTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, exportMenu.width, exportMenu.height);
                    exportPixelBuffer.resize(exportMenu.width * exportMenu.height * 4); 

                    std::string cmd = "ffmpeg -y -f rawvideo -pix_fmt bgra -s " + std::to_string(exportMenu.width) + "x" + std::to_string(exportMenu.height) + 
                                      " -r " + std::to_string(exportMenu.fps) + " -i - -i temp_audio.wav -c:v libx264 -preset fast -crf 23 -c:a aac -b:a 192k -pix_fmt yuv420p \"" + exportMenu.outputPath + "\"";
                    
                    #ifdef _WIN32
                    ffmpegPipe = _popen(cmd.c_str(), "wb");
                    #else
                    ffmpegPipe = popen(cmd.c_str(), "w");
                    #endif
                    if (!ffmpegPipe) {
                        isExporting = false;
                        if (exportTargetTexture) {
                            SDL_DestroyTexture(exportTargetTexture);
                            exportTargetTexture = nullptr;
                        }
                        std::filesystem::remove("temp_audio.wav");
                    }
                }
            }

            bool doSeek = false; bool doAddText = false; bool effectChanged = false; 
            
            if (!isExporting) {
                int mouseX, mouseY;
                Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
                bool inPreviewArea = (mouseX > leftPanelW) && (mouseX < WINDOW_VIEW_W - rightPanelW) && (mouseY < WINDOW_VIEW_H);
                
                if (inPreviewArea && (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) && !ImGui::GetIO().WantCaptureMouse) {
                    if (selectedClipIndex != -1 && selectedClipIndex < currentProject.clips.size()) {
                        float dx = mouseX - lastMouseX;
                        float dy = mouseY - lastMouseY;
                        
                        auto& clip = currentProject.clips[selectedClipIndex];
                        float len = clip.timelineEnd - clip.timelineStart;
                        float locProg = (len > 0.001f) ? (currentProgress - clip.timelineStart) / len : 0.0f;
                        
                        if (clip.animX.isAnimated) {
                            float curValX = clip.animX.GetValue(locProg, clip.posX);
                            clip.animX.AddOrUpdateKey(locProg, curValX + dx);
                        } else clip.posX += dx;

                        if (clip.animY.isAnimated) {
                            float curValY = clip.animY.GetValue(locProg, clip.posY);
                            clip.animY.AddOrUpdateKey(locProg, curValY + dy);
                        } else clip.posY += dy;
                    }
                }
                lastMouseX = mouseX; lastMouseY = mouseY;

                if (isPlaying) {
                    currentProgress += static_cast<float>(dt / maxDurationSec); 
                    if (currentProgress >= 1.0f) { currentProgress = 1.0f; isPlaying = false; }
                }
            } else {
                currentProgress = static_cast<float>(exportFrameCurrent) / exportFrameTotal;
                doSeek = true; 
            }

            std::string newFile = ui.Render(WINDOW_VIEW_W, WINDOW_VIEW_H, EXTRA_UI_HEIGHT, 
                                            currentProgress, isPlaying, doSeek, 
                                            currentProject.clips, selectedClipIndex, 
                                            showExportMenu, currentProject.tracks, 
                                            doAddText, effectChanged, currentProject.mediaFiles);
                                            
            while (players.size() < currentProject.tracks.size()) {
                players.push_back(std::make_unique<VideoPlayer>());
                lastActiveClipPerTrack.push_back(-1);
            }

            if (ui.triggerSave) { ui.triggerSave = false; if (currentProject.saveFilepath.empty()) ProjectManager::SaveProjectAs(currentProject); else ProjectManager::SaveProject(currentProject); }
            if (ui.triggerSaveAs) { ui.triggerSaveAs = false; ProjectManager::SaveProjectAs(currentProject); }
                                               
            if (!newFile.empty()) {
                if (std::find(currentProject.mediaFiles.begin(), currentProject.mediaFiles.end(), newFile) == currentProject.mediaFiles.end()) {
                    currentProject.mediaFiles.push_back(newFile);
                }
            }

            if (doAddText) {
                float start = currentProgress; float end = std::clamp(start + 0.15f, 0.0f, 1.0f); 
                if (currentProject.clips.empty()) { start = 0.0f; end = 1.0f; }
                VideoClip newClip("", start, end, 1, true, "YOUR TEXT HERE");
                if (TrackManager::AddClip(currentProject.clips, newClip)) {
                    selectedClipIndex = currentProject.clips.size() - 1;
                }
            }

            // --- БАЗА ИЕРАРХИИ: СБРОС И ПРОСЧЕТ МАТРИЦ ---
            for (auto& clip : currentProject.clips) clip.transformCalculatedThisFrame = false;

            auto ComputeGlobalTransform = [&](uint32_t clipId, auto& ComputeRef, int depth = 0) -> TransformMatrix {
                if (depth > 100) { LOG_ERROR("Infinite recursion detected in parenting!"); return TransformMatrix(); }
                auto it = std::find_if(currentProject.clips.begin(), currentProject.clips.end(), [clipId](const VideoClip& c) { return c.id == clipId; });
                if (it == currentProject.clips.end()) return TransformMatrix();

                VideoClip& clip = *it;
                if (clip.transformCalculatedThisFrame) return clip.globalTransform;

                float len = clip.timelineEnd - clip.timelineStart;
                float localProg = (len > 0.001f) ? (currentProgress - clip.timelineStart) / len : 0.0f;
                localProg = std::clamp(localProg, 0.0f, 1.0f);

                float lx = clip.animX.GetValue(localProg, clip.posX);
                float ly = clip.animY.GetValue(localProg, clip.posY);
                float lRot = clip.animRot.GetValue(localProg, clip.rotation);
                float lScale = clip.animScale.GetValue(localProg, clip.scale);

                TransformMatrix localMat = TransformMatrix::CreateTRS(lx, ly, lRot, lScale);

                if (clip.parentId != 0) {
                    TransformMatrix parentGlobal = ComputeRef(clip.parentId, ComputeRef, depth + 1);
                    clip.globalTransform = parentGlobal * localMat; 
                } else {
                    clip.globalTransform = localMat;
                }

                clip.transformCalculatedThisFrame = true;
                return clip.globalTransform;
            };

            for (auto& clip : currentProject.clips) {
                if (currentProgress >= clip.timelineStart && currentProgress < clip.timelineEnd) {
                    ComputeGlobalTransform(clip.id, ComputeGlobalTransform, 0);
                }
            }

            int previewW = viewW;
            int previewH = WINDOW_VIEW_H;

            int drawViewX = leftPanelW;
            int drawViewY = 0;
            int drawViewW = previewW;
            int drawViewH = previewH;
            
            if (isExporting) {
                drawViewX = 0;
                drawViewY = 0;
                drawViewW = exportMenu.width;
                drawViewH = exportMenu.height;
                if (exportTargetTexture) {
                    SDL_SetRenderTarget(renderer, exportTargetTexture);
                }
            } else {
                SDL_SetRenderTarget(renderer, nullptr);
            }

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            
            for (int t = 0; t < currentProject.tracks.size(); ++t) {
                int activeClipIndex = -1;
                for (int i = 0; i < currentProject.clips.size(); ++i) {
                    if (currentProject.clips[i].trackIndex == t && 
                        currentProgress >= currentProject.clips[i].timelineStart && 
                        currentProgress <= currentProject.clips[i].timelineEnd) {
                        
                        // Prevent out-of-bounds rendering (strict less-than normally) but allow boundary match if selected
                        if (currentProgress < currentProject.clips[i].timelineEnd || i == selectedClipIndex) {
                            if (activeClipIndex == -1 || i == selectedClipIndex) {
                                activeClipIndex = i;
                            }
                        }
                    }
                }

                if (activeClipIndex != -1) {
                    VideoClip& activeClip = currentProject.clips[activeClipIndex];
                    
                    float finalX, finalY, finalRot, finalScale;
                    activeClip.globalTransform.Decompose(finalX, finalY, finalRot, finalScale);

                    float drawX = isExporting ? finalX * ((float)exportMenu.width / previewW) : finalX;
                    float drawY = isExporting ? finalY * ((float)exportMenu.height / previewH) : finalY;

                    if (activeClip.isNullObject) {
                        players[t]->isPlaying = false;
                        if (lastActiveClipPerTrack[t] != -1) players[t]->ClearAudio();
                    }
                    else if (activeClip.isText) {
                        players[t]->isPlaying = false;
                        if (lastActiveClipPerTrack[t] != -1) players[t]->ClearAudio(); 
                        
                        if (g_Font) {
                            int textW = 0, textH = 0;
                            SDL_Surface* textSurf = TTF_RenderUTF8_Blended(g_Font, activeClip.textContent.c_str(), {255, 255, 255, 255});
                            if (textSurf) {
                                SDL_Texture* textTex = SDL_CreateTextureFromSurface(renderer, textSurf);
                                if (textTex) {
                                    SDL_QueryTexture(textTex, nullptr, nullptr, &textW, &textH);
                                    float textScaleFactor = (float)drawViewH / 720.0f;
                                    float finalW = textW * finalScale * textScaleFactor;
                                    float finalH = textH * finalScale * textScaleFactor;
                                    
                                    float dstX = drawViewX + (drawViewW) / 2.0f + drawX - finalW / 2.0f;
                                    float dstY = drawViewY + (drawViewH) / 2.0f + drawY - finalH / 2.0f;
                                    SDL_Rect rect = { (int)dstX, (int)dstY, (int)finalW, (int)finalH };
                                    
                                    SDL_Surface* shadowSurf = TTF_RenderUTF8_Blended(g_Font, activeClip.textContent.c_str(), {0, 0, 0, 255});
                                    if (shadowSurf) {
                                        SDL_Texture* shadowTex = SDL_CreateTextureFromSurface(renderer, shadowSurf);
                                        if (shadowTex) {
                                            SDL_Rect shadowRect = { rect.x + 2, rect.y + 2, rect.w, rect.h };
                                            SDL_RenderCopyEx(renderer, shadowTex, nullptr, &shadowRect, (double)finalRot, nullptr, SDL_FLIP_NONE);
                                            SDL_DestroyTexture(shadowTex);
                                        }
                                        SDL_FreeSurface(shadowSurf);
                                    }
                                    
                                    SDL_RenderCopyEx(renderer, textTex, nullptr, &rect, (double)finalRot, nullptr, SDL_FLIP_NONE);
                                    SDL_DestroyTexture(textTex);
                                }
                                SDL_FreeSurface(textSurf);
                            }
                        }
                    } 
                    else {
                        if (players[t]->loadedFilepath != activeClip.filepath) players[t]->LoadVideo(activeClip.filepath, renderer);
                        if (effectChanged) players[t]->textureNeedsUpdate = true;

                        float len = activeClip.timelineEnd - activeClip.timelineStart;
                        float localProg = (len > 0.001f) ? (currentProgress - activeClip.timelineStart) / len : 0.0f;
                        
                        float mediaProgress = activeClip.mediaStart + localProg * (activeClip.mediaEnd - activeClip.mediaStart);
                        double targetTimeSec = mediaProgress * players[t]->GetDurationSeconds();
                        bool isVideoTrack = (currentProject.tracks[t].type == TRACK_VIDEO);

                        if (doSeek) players[t]->Seek(mediaProgress, isVideoTrack);
                        else if (activeClipIndex != lastActiveClipPerTrack[t]) {
                            if (std::abs(targetTimeSec - players[t]->GetCurrentSec()) > 0.1) players[t]->Seek(mediaProgress, isVideoTrack);
                        }

                        players[t]->isPlaying = isExporting ? false : isPlaying; 
                        players[t]->currentVolume = activeClip.volume;

                        players[t]->UpdateAndDraw(renderer, drawViewX, drawViewY, drawViewW, drawViewH, targetTimeSec, isVideoTrack, 
                                                  drawX, drawY, finalScale, finalRot, activeClip.effects); 
                    }
                } 
                else {
                    players[t]->isPlaying = false;
                    if (lastActiveClipPerTrack[t] != -1) players[t]->ClearAudio(); 
                }
                lastActiveClipPerTrack[t] = activeClipIndex;
            }

            if (isExporting && ffmpegPipe) {
                SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_BGRA8888, exportPixelBuffer.data(), exportMenu.width * 4);
                fwrite(exportPixelBuffer.data(), 1, exportPixelBuffer.size(), ffmpegPipe);
                exportFrameCurrent++;
                
                SDL_SetRenderTarget(renderer, nullptr);
                SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
                SDL_RenderClear(renderer);

                ui.DrawSurface(renderer);
                SDL_SetRenderDrawColor(renderer, 50, 200, 50, 255);
                SDL_Rect progressRect = { 0, WINDOW_VIEW_H + EXTRA_UI_HEIGHT - 10, static_cast<int>(static_cast<float>(exportFrameCurrent) / exportFrameTotal * WINDOW_VIEW_W), 10 };
                SDL_RenderFillRect(renderer, &progressRect);
                
                if (exportFrameCurrent >= exportFrameTotal) {
                    isExporting = false;
                    #ifdef _WIN32
                      _pclose(ffmpegPipe);
                    #else
                       pclose(ffmpegPipe);
                    #endif
                    ffmpegPipe = nullptr;
                    currentProgress = 0.0f; 
                    
                    if (exportTargetTexture) {
                        SDL_DestroyTexture(exportTargetTexture);
                        exportTargetTexture = nullptr;
                    }
                    std::filesystem::remove("temp_audio.wav");
                }
            } else {
                exportMenu.Draw(&showExportMenu);
                ui.DrawSurface(renderer); 
            }
            SDL_RenderPresent(renderer);
        }
    } 

    if (isExporting && ffmpegPipe){
        #ifdef _WIN32
           _pclose(ffmpegPipe);
        #else
           pclose(ffmpegPipe);
        #endif
        ffmpegPipe = nullptr;
    }
    if (exportTargetTexture) {
        SDL_DestroyTexture(exportTargetTexture);
        exportTargetTexture = nullptr;
    }
    std::filesystem::remove("temp_audio.wav");

    if (g_Font) {
        TTF_CloseFont(g_Font);
        g_Font = nullptr;
    }
    TTF_Quit();

    PluginManager::Shutdown();
    ui.Shutdown(); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}