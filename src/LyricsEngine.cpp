#include "LyricsEngine.hpp"
#include <iostream>
#include <curl/curl.h>

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