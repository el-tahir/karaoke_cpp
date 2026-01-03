#include <iostream>
#include <LyricsEngine.hpp>

int main() {
    std::cout << "--- lyrics fetcher test ---" << std::endl;

    LyricsFetcher fetcher;

    std::string artist = "Adele";
    std::string title  = "Hello";

    auto result = fetcher.fetch_lyrics(artist, title);

    if (result) {
        std::cout << "\n [success] lyrics found! previewing first 200 chars: \n" << std::endl;
        std::cout << result->substr(0, 200) << "..." << std::endl;
    } else {
        std::cout << "\n [error] lyrics not found" << std::endl;
    }

    return 0;
}