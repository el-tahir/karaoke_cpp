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
    std::array<char, 4096> buffer;
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
    cmd << "yt-dlp --print \"%(artist)s\" --print \"%(track)s\" --print \"%(title)s\" "
        << "\"" << url << "\"";
    

    std::string output = run_command_with_output(cmd.str());

    if (output.empty()) return std::nullopt;

    //split output by newlines into a vector
    std::vector<std::string> lines;
    std::stringstream ss(output);
    std::string line;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back(); // handle windows formatting
        lines.push_back(line);
    }

    std::string meta_artist = (lines.size() > 0) ? lines[0] : "";
    std::string meta_track =  (lines.size() > 1) ? lines[1] : "";
    std::string full_title =  (lines.size() > 2) ? lines[2] : "";

    VideoMetadata meta;

    // trust the youtube music metadata
    // yt-dlp returns "NA" if the field is missing

    if (!meta_artist.empty() && meta_artist != "NA" && !meta_track.empty() && meta_track != "NA") {
        meta.artist = meta_artist;
        meta.title = meta_track;
        std::cout << "[metadata] used structured data: " << meta.artist << " - " << meta.title << std::endl;

    }
    
    else {
        std::cout << "[metadata] structured data missing. falling back to title parse." << std::endl;
        
        std::string target_title = full_title.empty() ? meta_artist : full_title;

        std::regex split_regex("^(.*?) - (.*)$");
        std::smatch match;

        if (std::regex_search(target_title, match, split_regex)) {
            meta.artist = match[1].str();
            meta.title = match[2].str();
        } else {
            meta.artist = "";
            meta.title = target_title;
        }
    }

    // clean up

    std::regex cleanup_regex(R"(\s*[\(\[](Official|Video|Audio|Lyrics|HD|4K|Remaster).*[\)\]])", std::regex::icase);
    meta.title = std::regex_replace(meta.title, cleanup_regex, "");

    // trim whitespace

    auto trim = [](std::string& s) {
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);
    };

    trim(meta.artist);
    trim(meta.title);

    return meta;
}



std::optional<fs::path> ExternalTools::download_audio(const std::string& url, const fs::path& out_path) {
    // cache check
    if (fs::exists(out_path)) {
        std::cout << "[cache hit] audio already downloaded." << std::endl;
        return out_path;
    }

    // ensure parent directory exists
    fs::create_directories(out_path.parent_path());


    // clean up wav file for parser
    std::stringstream cmd;
    cmd << "yt-dlp -x --audio-format wav "
        << "--postprocessor-args \"ffmpeg:-map_metadata -1 -fflags +bitexact -acodec pcm_s16le -ar 44100 -ac 2\" "
        << "--output \"" << out_path.string() << "\" "
        << "\"" << url << "\"";

    if (execute_command(cmd.str())) {
        if (fs::exists(out_path)) return out_path;
    }

    std::cerr << "[error] yt-dlp failed to download audio" << std::endl;
    return std::nullopt;
}

std::optional<fs::path> ExternalTools::run_separator(const fs::path& input_wav, const fs::path& out_path) {
    if (fs::exists(out_path)) {
        std::cout << "[cache hit] vocals already separated." << std::endl;
        return out_path;
    }

    std::stringstream cmd;
    cmd << paths_.separator_binary.string() << " "
        << "\"" << input_wav.string() << "\" "
        << "\"" << out_path.string() << "\"";
    
    
    if (execute_command(cmd.str())) {
        if (fs::exists(out_path)) return out_path;
    }

    std::cerr << "[error] custom separator failed" << std:: endl;
    return std::nullopt;
}

bool ExternalTools::render_video(const fs::path& audio_path,
                                const fs::path& ass_path,
                                const fs::path& out_path) {

    // ensure parent directory exsists
    fs::create_directories(out_path.parent_path());

    // ffmpeg command to combine audio + ass subtitles + black background
    std::stringstream cmd;
    cmd << "ffmpeg -y "
        << "-f lavfi -i color=c=black:s=1920x1080:r=30 " // black background
        << "-i \"" << audio_path.string() << "\" "       // audio input
        << "-vf \"ass=" << ass_path.string() << "\" "    // subtitle filter
        << "-shortest "                                  // stop when audio ends
        << "-c:v libx264 -c:a aac -b:a 192k "
        << "\"" << out_path.string() << "\"";

    return execute_command(cmd.str());

}


