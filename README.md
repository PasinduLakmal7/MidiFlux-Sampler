# 🎹 Modern MIDI Player & Sampler

A powerful, dark-themed MIDI player built with **JUCE (C++)**. This application features a "glassmorphism" interface, 8 interactive drum pads, a dynamic file browser, and integrated YouTube audio streaming.

---

## ✨ Features
-   **💎 Premium UI**: Dark mode with neon accents and glassy transparency effects.
-   **🥁 8 Interactive Pads**: Trigger sounds via MIDI (Notes 36-43) or by clicking.
-   **📂 Sidebar Browser**: Drag-and-drop navigation for folders and audio files.
-   **📺 YouTube Streaming**: Paste a link to instantly stream audio (requires `yt-dlp` and `ffmpeg`).
-   **🧩 Modular Architecture**: Refactored for maintainability and scalability.

---

## 🚀 Getting Started

### 1. Prerequisites
To use the YouTube streaming feature, you must have the following tools in the same folder as your executable:
-   **[yt-dlp](https://github.com/yt-dlp/yt-dlp)** (`yt-dlp.exe`)
-   **[ffmpeg](https://ffmpeg.org/download.html)** (`ffmpeg.exe`)

### 2. Building the Project
1.  Open the `.jucer` file in the **JUCE Projucer**.
2.  Select your preferred exporter (e.g., Visual Studio).
3.  Click "Save and Open in IDE".
4.  Build the solution in your IDE (Release mode recommended).

---

## 🛠️ Usage
1.  **Load a Sound**: Drag any `.wav` or `.mp3` from the sidebar onto a pad.
2.  **Play**: Click a pad or press a MIDI key (default: Kick note 36).
3.  **YouTube**: Paste a link and click **"Play YouTube"** to stream music in the background.

---
