#include <iostream>
#include <string>
#include <filesystem>
#include <ExternalTools.hpp>
#include <LyricsEngine.hpp>

int main(int argc, char* argv[]) {

    if (argc < 2) {
        std::cout << "usage: ./karaoke <youtube_url_or_search_term>" << std::endl;
        return 1;
    }

    std::string input = argv[1];

    // if input doesnt look like a url, treat it as a search
    if (input.find("http") == std::string::npos) {
        input = "ytsearch1:\"" + input + "\"";
    }


    std::cout << "--full pipeline--" << std::endl;
    
    ExternalTools tools;
    LyricsFetcher lyrics_fetcher;
    AssConverter ass_converter;

    // 1. get metadata
    std::cout << "\n [1/5] fetching metadata..." << std::endl;

    auto metadata_opt = tools.get_youtube_metadata(input);
    if (!metadata_opt) {
        std::cerr << "failed to get metadata" << std::endl;
        return 1;
    }

    VideoMetadata meta = *metadata_opt;

    std::cout << "  title:  " << meta.title << std::endl;
    std::cout << "  artist  " << meta.artist << std::endl;
    
    if (meta.artist.empty()) {
        std::cout << "artist detection failed. using title as query.." << std::endl;
    }

    // 2. download audio
    std::cout << "\n[2/5] downloading audio..." << std::endl;
    auto audio_path = tools.download_audio(input);
    if (!audio_path) return 1;

    // 3. separate audio 

    std::cout << "\n[3/5] separating vocals..." << std::endl;
    auto final_audio_path = *audio_path;

    // check if separator binary exists
    if (std::filesystem::exists("./separator")) {
        auto separated_path = tools.run_separator(*audio_path);
        if (separated_path) {
            final_audio_path = *separated_path;
            std::cout << " separation complete" << std::endl;
        } else {
            std::cerr << " separation failed. using original audio" << std::endl;
        }
    } else {
        std::cerr << " separator binary not found, using original audio" << std::endl;
    }

    // fetch and process lyrics

    std::cout << "\n[4/5] fetching lyrics..." << std::endl;

    auto lrc_opt = lyrics_fetcher.fetch_lyrics(meta.artist, meta.title);

    if (!lrc_opt && meta.artist.empty()) {
        // try fetching with just title if artist is empty
        lrc_opt = lyrics_fetcher.fetch_lyrics("", meta.title);
    }

    if (!lrc_opt) {
        std::cerr << "lyrics not found. proceeding with instrumental video" << std::endl;
        lrc_opt = "[00:00.00] (Instrumental / Lyrics not found)";
    }

    std::cout << " parsing lyrics..." << std::endl;

    auto lines = ass_converter.parse_lrc(*lrc_opt);

    std::string ass_content = ass_converter.generate_ass(lines);
    std::filesystem::path ass_path = "temp/karaoke.ass";
    ass_converter.save_to_file(ass_content, ass_path);

    // 5. render video
    std::cout << "\n[5/5] rendering final video..." << std::endl;
    std::string output_filename = "karaoke_" + std::to_string(std::time(nullptr)) + ".mp4";

    if (tools.render_video(final_audio_path, ass_path, output_filename)) {
        std::cout << "\n-----done!-----" << std::endl;
        std::cout << "file saved to output/" << output_filename << std::endl;

    } else {
        std::cerr << "rendering failed" << std::endl;
        return 1;
    }



    return 0;
}