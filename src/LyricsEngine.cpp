#include "LyricsEngine.hpp"
#include <iostream>
#include <curl/curl.h>
#include <sstream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <cmath>

using json = nlohmann::json;

// helper : Write Callback for LibCurl
// this accumualtes the data recieved from the server into a std::string

size_t LyricsFetcher::WriteCallBack(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// helper : url encoding
// converts spaces to %20, etc..

std::string LyricsFetcher::url_encode(const std::string& value) {
    CURL* curl = curl_easy_init();
    if (curl) {
        char* output = curl_easy_escape(curl, value.c_str(), value.length());
        std::string result(output);
        curl_free(output);
        curl_easy_cleanup(curl);
        return result;
    }
    return "";
}

// helper :: perform http get

std::string LyricsFetcher::perform_get_request(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallBack);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // some apis require a user-agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "karaoke_cpp");

        // follow redirects if any
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        
    }

    return readBuffer;
}

// main method : fetch lyrics

std::optional<std::string> LyricsFetcher::fetch_lyrics(const std::string& artist, const std::string& title) {
    // contruct the api url
    // endpoint https://lrclib.net/api/get?artist_name=...&track_name=...

    std::string query_url = "https://lrclib.net/api/get?artist_name=" + url_encode(artist) +
                            "&track_name=" + url_encode(title);
    
    std::cout << "[network] fetching lyrics from: " << query_url << std::endl;

    std::string response = perform_get_request(query_url);

    if (response.empty()) {
        std::cerr << "[network] empty response from server." << std::endl;
        return std::nullopt;
    }

    try {
        auto json_data = json::parse(response);

        //1. try to get synced lyrics (lrc format)

        if (json_data.contains("syncedLyrics") && !json_data["syncedLyrics"].is_null()) {
            std::string lyrics = json_data["syncedLyrics"].get<std::string>();
            if (!lyrics.empty()) return lyrics;

        }

        //2. fallback: check if only plain lyrics exist (for debugging)

        if (json_data.contains("plainLyrics") && !json_data["plainLyrics"].is_null()) {
            std::cerr << "[network] warning: only plain text lyrics found (no timestamps)." << std::endl;
            return std::nullopt;
        }
    } catch (const std::exception& e) {
        std::cerr << "[network] json parsing error: " << e.what() << std::endl;
        std::cerr << "raw response: " << response << std::endl;
    }

    return std::nullopt;

}


double AssConverter::parse_time_lrc(const std::string& timestamp) {
    // format mm::ss.xx

    try {
        int minutes = std::stoi(timestamp.substr(0, 2));
        double seconds = std::stod(timestamp.substr(3));
        return (minutes * 60.0) + seconds;
    } catch (...) {
        return 0.0;
    }
}

std::string AssConverter::format_time_ass(double seconds) {
    // format h:mm:ss.cc

    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>((seconds - (h * 3600)) / 60);
    int s = static_cast<int>(seconds) % 60;
    int cs = static_cast<int>((seconds - static_cast<int>(seconds)) * 100);

    char buffer[32];

    snprintf(buffer, sizeof(buffer), "%d:%02d:%02d.%02d", h, m, s, cs);
    return std::string(buffer);

}


std::vector<LyricLine> AssConverter::parse_lrc(const std::string& lrc_content) {
    std::vector<LyricLine> lines;
    std::stringstream ss(lrc_content);
    std::string segment;

    // regex for line [mm:ss.xx]text

    std::regex line_regex(R"(\[(\d{2}:\d{2}\.\d{2})\](.*))");

    // regex for word : <mm:ss.xx>text (optional, non-greedy match for text)

    std::regex word_regex(R"(<(\d{2}:\d{2}\.\d{2})>([^<]*))");

    while (std::getline(ss, segment)) {
        // strip carriage returns (\r) just in case

        if (!segment.empty() && segment.back() == '\r') segment.pop_back();

        if (segment.empty()) continue;

        std::smatch line_match;

        if (std::regex_search(segment, line_match, line_regex)) {
            LyricLine line;
            line.start_time = parse_time_lrc(line_match[1].str());
            std::string text_content = line_match[2].str();

            // check for word-level timestamps

            std::sregex_iterator iter(text_content.begin(), text_content.end(), word_regex);
            std::sregex_iterator end;

            if (iter != end) {
                line.is_word_level = true;

                while (iter != end) {
                    std::smatch word_match = *iter;
                    WordSegment word;
                    word.start_time = parse_time_lrc(word_match[1].str());
                    word.text = word_match[2].str();

                    // simple clean up of whitespace
                    if (word.text.empty()) word.text = " ";

                    line.words.push_back(word);
                    ++iter;
                }

                // calculate word durations

                for (size_t i = 0; i < line.words.size(); ++i) {
                    if (i + 1 < line.words.size()) {
                        line.words[i].end_time = line.words[i + 1].start_time;
                    } else {
                        // for the last word, we assume a short duration
                        // we will fix times when we process the full line vector
                        line.words[i].end_time = line.words[i].start_time + 0.5;
                    }
                }
            } else {
                line.text = text_content;
            }

            lines.push_back(line);
        }
    }

    // post-process: set line end times and fix last word durations

    for (size_t i = 0; i < lines.size(); ++i) {
        if (i + 1 < lines.size()) {
            lines[i].end_time = lines[i + 1].start_time;
        } else {
            lines[i].end_time = lines[i].start_time + 5.0; // default 5s padding for last line
        }

        // if word level, fix the last word's end time
        if (lines[i].is_word_level && !lines[i].words.empty()) {
            lines[i].words.back().end_time = lines[i].end_time;
        }
    }

    return lines;

}

std::string AssConverter::generate_header() {
    std::stringstream ss;
    ss << "[Script Info]\n"
       << "Title: Karaoke++ Subtitles\n"
       << "ScriptType: v4.00+\n"
       << "WrapStyle: 0\n"
       << "ScaledBorderAndShadow: yes\n"
       << "YCbCr Matrix: TV.601\n"
       << "PlayResX: " << config_.resolution_x << "\n"
       << "PlayResY: " << config_.resolution_y << "\n\n"
       << "[V4+ Styles]\n"
       << "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, "
       << "Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, "
       << "Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
       // current line style (white, larger)
       << "Style: KaraokeCurrent," << config_.font_name << "," << config_.font_size_current 
       << ",&H00FFFFFF,&H00FFFFFF,&H00000000,&H00000000,-1,0,0,0,100,100,0,0,1,3,3,2,10,10,10,1\n"
       // next line style (faded white)
       << "Style: KaraokeNext," << config_.font_name << "," << config_.font_size_next 
       << ",&H88FFFFFF,&H00FFFFFF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n"
       // next2 line style (more faded)
       << "Style: KaraokeNext2," << config_.font_name << "," << config_.font_size_next2
       << ",&H66FFFFFF,&H00FFFFFF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,1,1,2,10,10,10,1\n\n"
       << "[Events]\n"
       << "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";
    return ss.str();
}

std::string AssConverter::generate_karaoke_text(const LyricLine& line) {
    if (!line.is_word_level) return line.text;

    std::stringstream ss;

    for (const auto& word : line.words) {
        // duration in centiseconds

        int duration_cs = static_cast<int> ((word.end_time - word.start_time) * 100);
        ss << "{\\k}" << duration_cs << "}" << word.text;
    }

    return ss.str();

}

std::string AssConverter::generate_ass(const std::vector<LyricLine>& lines) {
    std::stringstream ss;
    ss << generate_header();

    double trans_dur = config_.transition_duration;

    int trans_ms = static_cast<int>(trans_dur * 1000);

    for (size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];

        // ensure duration is at least 0.1s

        double duration = std::max(0.1, line.end_time - line.start_time);
        int dur_ms = static_cast<int>(duration * 1000);

        // animation timing logic (move y position)

        int effective_trans_ms = std::min(trans_ms, dur_ms / 2);
        int move_start_ms = (dur_ms > effective_trans_ms) ? (dur_ms - effective_trans_ms) : 0;

        std::string start_ts = format_time_ass(line.start_time);
        std::string end_ts = format_time_ass(line.end_time);

        // 1. current line event

        std::string text_content = generate_karaoke_text(line);

        // {\move(x1,y1,x2,y2,t1,t2)\fad(t1,t2)}
        ss << "Dialogue: 0," << start_ts << "," << end_ts << ",KaraokeCurrent,,0,0,0,,"
           << "{\\move(960,520,960,400," << move_start_ms << "," << dur_ms << ")"
           << "\\fad(0," << effective_trans_ms << ")}" 
           << text_content << "\n";

        // 2. next line event (preview)
        if (i + 1 < lines.size()) {
            std::string next_text = lines[i+1].is_word_level ? 
                                    [&](){ 
                                        std::string s; 
                                        for(auto& w : lines[i+1].words) s += w.text; 
                                        return s; 
                                    }() 
                                    : lines[i+1].text;

            ss << "Dialogue: 0," << start_ts << "," << end_ts << ",KaraokeNext,,0,0,0,,"
               << "{\\move(960,660,960,520," << move_start_ms << "," << dur_ms << ")}"
               << next_text << "\n";
        }

        // 3. next2 line event (preview)
        if (i + 2 < lines.size()) {
            std::string next2_text = lines[i+2].is_word_level ? 
                                     [&](){ 
                                        std::string s; 
                                        for(auto& w : lines[i+2].words) s += w.text; 
                                        return s; 
                                    }() 
                                     : lines[i+2].text;

            ss << "Dialogue: 0," << start_ts << "," << end_ts << ",KaraokeNext2,,0,0,0,,"
               << "{\\move(960,800,960,660," << move_start_ms << "," << dur_ms << ")"
               << "\\fad(" << effective_trans_ms << ",0)}"
               << next2_text << "\n";
        }
    }

    return ss.str();
}


void AssConverter::save_to_file(const std::string& content, const std::filesystem::path& path) {

    std::ofstream out(path);

    if (out.is_open()) {
        out << content;
        out.close();
    } else {
        std::cerr << "failed to open file for writing: " << path << std::endl;
    }

}






