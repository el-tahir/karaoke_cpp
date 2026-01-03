#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

struct VideoMetadata {
    std::string title;
    std::string artist; // we try and guess this from the title
};



struct Paths {
        std::filesystem::path separator_binary = "./separator";
        std::filesystem::path temp_dir = "temp";
        std::filesystem::path output_dir = "output";
    };

class ExternalTools {
public:
    

    ExternalTools(Paths paths = Paths()) : paths_(paths) {
        std::filesystem::create_directories(paths_.temp_dir);
        std::filesystem::create_directories(paths_.output_dir);
    }

    // 1. get title/artist from youtube url
    std::optional<VideoMetadata> get_youtube_metadata(const std::string& url);

    // 2. downlaod audio via yt-dlp (returns path to downloaded .wav)

    std::optional<std::filesystem::path> download_audio(const std::string& url);


    // 3. run custom separator (returns path to intrumental .wav)
    std::optional<std::filesystem::path> run_separator(const std::filesystem::path& input_wav);

    // 4. render final video via ffmpeg

    bool render_video(const std::filesystem::path& audio_path,
                      const std::filesystem::path& ass_path, 
                      const std::string& output_filename);

private:
    Paths paths_;

    bool execute_command(const std::string& cmd);

    std::string run_command_with_output(const std::string& cmd);

};