#include "ExternalTools.hpp"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <array>
#include <memory>
#include <regex>

namespace fs = std::filesystem;

// helper : run command and capture output

std::string ExternalTools::run_command_with_output(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;

    // open pipe to command

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";

    // read stdout

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // remove trailing newline

    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }

    return result;

}

bool ExternalTools::execute_command(const std::string& cmd) {
    std::cout << "[exec]" << cmd << std::endl;
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

// --metadata extraction--

std::optional<VideoMetadata> ExternalTools::get_youtube_metadata(const std::string& url) {
    // ask yt-dlp for the full title
    std::stringstream cmd;
    cmd << "yt-dlp --print title \"" << url << "\"";
    std::string full_title = run_command_with_output(cmd.str());

    if (full_title.empty()) return std::nullopt;

    VideoMetadata meta;

    // try to split by " - "
    
    std::regex split_regex("^(.*?) - (.*)$");
    std::smatch match;

    if (std::regex_search(full_title, match, split_regex)) {
        meta.artist = match[1].str();
        meta.title = match[2].str();
    } else {
        // fallback: use the whole thing as title
        meta.artist = "";
        meta.title = full_title;
    }

    // clean up "official audio", "(audio)", etc..
    std::regex cleanup_regex(R"(\s*[\(\[](Official|Video|Audio|Lyrics|HD|4K).*[\)\]])", std::regex::icase);
    meta.title = std::regex_replace(meta.title, cleanup_regex, "");

    return meta;


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


