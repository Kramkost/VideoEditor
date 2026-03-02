# 🎬 Titan Video Editor

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=for-the-badge&logo=c%2B%2B)
![SDL2](https://img.shields.io/badge/Windowing-SDL2-orange.svg?style=for-the-badge)
![Dear ImGui](https://img.shields.io/badge/GUI-Dear_ImGui-9cf.svg?style=for-the-badge)
![FFmpeg](https://img.shields.io/badge/Media-FFmpeg-41ba25.svg?style=for-the-badge&logo=ffmpeg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg?style=for-the-badge)
![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)

**Titan Video Editor** is a custom, lightweight, and blazingly fast video editing suite built entirely from scratch in C++. 

Unlike many modern editors that act as heavy frontend wrappers, Titan features its own deeply optimized render pipeline and native project management system. Designed to be a high-performance alternative to bloated commercial software, it delivers zero-latency UI feedback, multithreaded processing, and pixel-perfect control over your media.

Created by **Kramkost**.

---

## ✨ Key Features

* **🎞️ Advanced Infinite Timeline:** * Add unlimited video and audio tracks.
    * Seamless drag-and-drop support directly from your OS into the project bin or timeline.
    * Instant clip splitting (cutting) and fluid, stutter-free playback.
* **📈 Bezier Curve Animation:** * Achieve motion-graphics-level precision with full keyframe control over clip position, scale, and rotation.
    * Customizable easing curves: *Linear, Smooth, Ease In, Ease Out*.
* **⚡ Dynamic Plugin System (`.dll` / `.so`):** * Extend the editor's capabilities with real-time visual effects via external C++ plugins. 
    * Frame processing is completely multithreaded across all CPU cores to guarantee maximum FPS during playback and rendering.
* **💾 Custom Serialization (`.titansave`):** * A proprietary, lightning-fast project format. Instantly save and load your media paths, timeline clips, and animation data without waiting for heavy project files to parse.
* **🎮 Hardware Accelerated UI:** * Built on top of Dear ImGui and SDL2, the interface guarantees zero-latency interactions. What you click is what happens, instantly.
* **🚀 Rock-Solid Export Pipeline:** * Renders frames directly from the GPU buffer into an FFmpeg process via a seamless pipe (`popen`). 
    * Guarantees a 100% visual match between your editor preview and the final `.mp4` file.
    * Supports arbitrary resolutions, including FullHD, 4K, and vertical formats (1080x1920) for Shorts/Reels.

---

## 📸 Gallery

![Editor Interface Placeholder][Insert Screenshot Here]
> *Titan's primary editing interface, showcasing the infinite timeline and hardware-accelerated preview window.*

![Animation Curves Placeholder][Insert Screenshot Here]
> *Fine-tuning Bezier curves for smooth, professional keyframe animations.*

---

## 🛠️ Tech Stack

* **Core Language:** C++17
* **Windowing & Input:** SDL2
* **Graphical User Interface:** Dear ImGui
* **Media Decoding & Pipeline:** FFmpeg (`libavcodec`, `libavformat`, `libswscale`, and CLI pipe)
* **Build System:** CMake (Cross-platform support for Windows and Linux)

---

## ⚙️ Getting Started

### Prerequisites

Before building, ensure you have the following installed on your system:
* **CMake** (v3.15 or higher)
* A C++17 compatible compiler (GCC, Clang, or MSVC)
* **SDL2** development libraries
* **FFmpeg** development libraries (and the FFmpeg CLI available in your system PATH for exporting)

### Building from Source (Windows & Linux)

Titan uses CMake to ensure a smooth, cross-platform compilation process. Run the following commands in your terminal:

```bash
# 1. Clone the repository
git clone [https://github.com/YourUsername/TitanVideoEditor.git](https://github.com/YourUsername/TitanVideoEditor.git)
cd TitanVideoEditor

# 2. Create a build directory
mkdir build
cd build

# 3. Generate build files
cmake ..

# 4. Compile the project
cmake --build . --config Release
