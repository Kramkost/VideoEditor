#pragma once
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "plugins.h"

enum TrackType {
    TRACK_VIDEO,
    TRACK_AUDIO
};

struct TimelineTrack {
    std::string name;
    TrackType type;
};

// ТИПЫ КРИВЫХ АНИМАЦИИ (BEZIER)
enum EasingType {
    EASE_LINEAR,     
    EASE_SMOOTH,     
    EASE_IN,         
    EASE_OUT         
};

struct Keyframe {
    float time;  
    float value; 
};

struct AnimTrack {
    bool isAnimated = false;
    EasingType easing = EASE_SMOOTH; 
    std::vector<Keyframe> keys;

    void AddOrUpdateKey(float time, float val) {
        for (auto& k : keys) {
            if (std::abs(k.time - time) < 0.01f) { k.value = val; return; }
        }
        keys.push_back({time, val});
        std::sort(keys.begin(), keys.end(), [](const Keyframe& a, const Keyframe& b){ return a.time < b.time; });
    }

    float GetValue(float time, float defaultVal) const {
        if (!isAnimated || keys.empty()) return defaultVal;
        if (keys.size() == 1) return keys[0].value;
        if (time <= keys.front().time) return keys.front().value;
        if (time >= keys.back().time) return keys.back().value;

        for (size_t i = 0; i < keys.size() - 1; ++i) {
            if (time >= keys[i].time && time <= keys[i+1].time) {
                float t = (time - keys[i].time) / (keys[i+1].time - keys[i].time);
                
                float smoothT = t;
                switch (easing) {
                    case EASE_LINEAR: smoothT = t; break;
                    case EASE_SMOOTH: smoothT = t * t * (3.0f - 2.0f * t); break;
                    case EASE_IN:     smoothT = t * t * t; break;
                    case EASE_OUT:    smoothT = 1.0f - std::pow(1.0f - t, 3.0f); break;
                }
                
                return keys[i].value + (keys[i+1].value - keys[i].value) * smoothT;
            }
        }
        return defaultVal;
    }
};

struct VideoClip {
    std::string filepath;
    float timelineStart; 
    float timelineEnd;   
    float mediaStart;    
    float mediaEnd;      
    float volume;    
    int trackIndex; 
    
    float posX; float posY; float scale; float rotation; 
    AnimTrack animX, animY, animScale, animRot;

    bool isText;
    std::string textContent;
    std::vector<EffectParams> effects;

    // Для плавной анимации интерфейса (Hover/Click)
    float visualScale; 
    bool isInteracting;

    VideoClip(std::string path, float tStart, float tEnd, int track, bool text = false, std::string tContent = "") 
        : filepath(path), timelineStart(tStart), timelineEnd(tEnd), mediaStart(0.0f), mediaEnd(1.0f), 
          volume(1.0f), trackIndex(track), posX(0.0f), posY(0.0f), scale(1.0f), rotation(0.0f), 
          isText(text), textContent(tContent), visualScale(1.0f), isInteracting(false) {}
};

class UIManager {
public:
    void Init(SDL_Window* window, SDL_Renderer* renderer);
    void ProcessEvent(const SDL_Event* event);
    
    std::string Render(int windowW, int windowH, int uiHeight, 
                       float& progress, bool& isPlaying, bool& doSeek,
                       std::vector<VideoClip>& clips, int& selectedClipIndex, 
                       bool& showExport, std::vector<TimelineTrack>& tracks,
                       bool& doAddText, bool& effectChanged, 
                       std::vector<std::string>& projectFiles);
                       
    void DrawSurface(SDL_Renderer* renderer);
    void Shutdown();
    
    // --- НОВЫЕ ФЛАГИ ДЛЯ СОХРАНЕНИЯ ---
    bool triggerSave = false;
    bool triggerSaveAs = false;
    
private:
    std::string OpenFileDialog();
    
    bool showSettings = false; 
};