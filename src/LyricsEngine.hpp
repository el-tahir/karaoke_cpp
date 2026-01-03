#pragma once

#include <string>
#include <vector>
#include <optional>
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






