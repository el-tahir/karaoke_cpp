#include <iostream>
#include <filesystem>
#include <LyricsEngine.hpp>

int main() {
    std::cout << "--- subtitle generator test ---" << std::endl;

    LyricsFetcher fetcher;
    AssConverter converter;

    std::string artist = "Adele";
    std::string title  = "Hello";

    // 1. fetch

    auto lrc_opt = fetcher.fetch_lyrics(artist, title);

    if (!lrc_opt) {
        std::cerr << "[error] failed to fetch lyrics" << std::endl;
        return 1;
    }

    std::cout << "[success] fetched lyrics" << std::endl;

    // 2. parse

    auto lines = converter.parse_lrc(*lrc_opt);

    std::cout << "[success] parsed" << lines.size() << " lines" << std::endl;

    // 3. generate ass

    std::string ass_content = converter.generate_ass(lines);
    std::cout << "[success] generated ass content." << std::endl;

    // 4. save
    std::filesystem::path out_path = "adele_hello.ass";

    converter.save_to_file(ass_content, out_path);
    std::cout << "[success] saved to " << out_path << std::endl; 

    return 0;
}