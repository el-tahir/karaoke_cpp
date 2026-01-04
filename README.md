# karaoke_cpp

A C++ tool that generates karaoke videos from YouTube links or search terms.

## What it does

1. Downloads audio from YouTube
2. Separates vocals from instrumentals using MDX-Net
3. Fetches synced lyrics automatically
4. Renders karaoke-style videos with subtitles

## Requirements

- CMake 3.10+
- libcurl
- nlohmann_json
- yt-dlp (for downloading)
- ffmpeg (for video rendering)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

Run from the project root directory:

```bash
./build/karaoke "https://music.youtube.com/watch?v=..."
# or search by name
./build/karaoke "Artist - Song Name"
```

> **Tip:** Use YouTube Music links for better metadata and lyrics matching.

Output videos are saved to the `output/` directory.

## Model Download

Download the vocal separation model:

```bash
mkdir -p models
wget -O models/UVR_MDXNET_KARA_2.onnx https://huggingface.co/AI4future/RVC/resolve/main/UVR_MDXNET_KARA_2.onnx
```

## Optional: Vocal Separator

Place the `separator` binary (from https://github.com/el-tahir/mdxnet_cpp) in the project root to enable vocal separation. Without it, the original audio is used.
