#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>

// data structures

struct WordSegment {
    double start_time = 0.0;
    double end_time = 0.0;
    std::string text;
};

struct LyricLine {
    double start_time = 0.0;
    double end_time = 0.0;
    std::string text;
    std::vector<WordSegment> words;
    bool is_word_level = false;
};

// network layer

class LyricsFetcher {
public:
    // fetches raw LRC string from LRCLIB
    std::optional<std::string> fetch_lyrics(const std::string& artist, const std::string& title);

private:
    // lib curl helper functions
    static size_t WriteCallBack(void* contents, size_t size, size_t nmemb, std::string* userp);
    std::string url_encode(const std::string& value);
    std::string perform_get_request(const std::string& url);

};

struct AssConfig {
        int resolution_x = 1920;
        int resolution_y = 1080;
        std::string font_name = "Montserrat";
        int font_size_current = 60;
        int font_size_next = 60;
        int font_size_next2 = 60;
        double transition_duration = 0.3;
    };


class AssConverter {
public:
    
    AssConverter(AssConfig config = AssConfig()) : config_(config) {}

    // parses raw lrc string into structed data
    std::vector<LyricLine> parse_lrc(const std::string& lrc_content);

    // generates the full .ass file content
    std::string generate_ass(const std::vector<LyricLine>& lines);

    void save_to_file(const std::string& content, const std::filesystem::path& path);

private:
    AssConfig config_;

    std::string format_time_ass(double seconds);
    double parse_time_lrc(const std::string& timestamp);
    std::string generate_header();
    std::string generate_karaoke_text(const LyricLine& line);

};






