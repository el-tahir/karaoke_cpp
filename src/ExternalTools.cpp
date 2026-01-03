#include "ExternalTools.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>

namespace fs = std::filesystem;

bool ExternalTools::execute_command(const std::string& cmd) {
    std::cout << "[exec]" << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

std::optional<fs::path> ExternalTools::download_audio(const std::string& url) {
    // target temp/downloaded.wav

    fs::path output_path = paths_.temp_dir / "downloaded.wav";

    std::stringstream cmd;

    cmd << "yt-dlp -x --audio-format wav "
        << "--output \"" << output_path.string() << "\" "
        << "\"" << url << "\"";

    if (execute_command(cmd.str())) {
        if (fs::exists(output_path)) return output_path;
    }

    std::cerr << "[error] yt-dlp failed to download audio" << std::endl;
    return std::nullopt;
}

std::optional<fs::path> ExternalTools::run_separator(const fs::path& input_wav) {
    // usage ./separator <input.wav> <output.wav>
    fs::path output_instrumental = paths_.temp_dir / "instrumental.wav";

    std::stringstream cmd;
    cmd << paths_.separator_binary.string() << " "
        << "\"" << input_wav.string() << "\" "
        << "\"" << output_instrumental.string() << "\"";
    
    
    if (execute_command(cmd.str())) {
        if (fs::exists(output_instrumental)) return output_instrumental;
    }

    std::cerr << "[error] custom separator failed" << std:: endl;
    return std::nullopt;
}

bool ExternalTools::render_video(const fs::path& audio_path,
                                const fs::path& ass_path,
                                const std::string& output_filename) {
    
    fs::path output_path = paths_.output_dir / output_filename;

    // ffmpeg command to combine audio + ass subtitles + black background
    std::stringstream cmd;
    cmd << "ffmpeg -y "
        << "-f lavfi -i color=c=black:s=1920x1080:r=30 " // black background
        << "-i \"" << audio_path.string() << "\" "       // audio input
        << "-vf \"ass=" << ass_path.string() << "\" "    // subtitle filter
        << "-shortest "                                  // stop when audio ends
        << "-c:v libx264 -c:a aac -b:a 192k "
        << "\"" << output_path.string() << "\"";

    return execute_command(cmd.str());


}


