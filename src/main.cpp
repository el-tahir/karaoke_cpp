#include <iostream>
#include <string>
#include <filesystem>
#include <ExternalTools.hpp>
#include <LyricsEngine.hpp>

namespace fs = std::filesystem;

std::string sanitize_filename(std::string name) {
    std::string invalid_chars = "\\/:?\"<>|";
    for (char& c : name) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = '_';
        }
    }

    return name;
}

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

    // create hash of input string to use as folder name
    size_t input_hash = std::hash<std::string>{}(input);
    std::string project_id = std::to_string(input_hash);

    fs::path project_dir = fs::path("artifacts") / project_id;
    fs::create_directories(project_dir);

    fs::path p_source_wav =       project_dir / "source.wav";
    fs::path p_instrumental_wav = project_dir / "instrumental.wav";
    fs::path p_subtitles_ass =    project_dir / "karaoke.ass";

    std::cout << "project id: " << project_id << std::endl;
    std::cout << "artificats: " << project_dir << std::endl;


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
    auto audio_path = tools.download_audio(input, p_source_wav);
    if (!audio_path) return 1;

    // 3. separate audio 

    std::cout << "\n[3/5] separating vocals..." << std::endl;
    auto final_audio_path = *audio_path;

    // check if separator binary exists
    if (std::filesystem::exists("./separator")) {
        auto separated_path = tools.run_separator(*audio_path, p_instrumental_wav);
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
    ass_converter.save_to_file(ass_content, p_subtitles_ass);

    // 5. render videos
    std::string safe_title = sanitize_filename(meta.title);
    if (!meta.artist.empty()) safe_title = sanitize_filename(meta.artist) + " - " + safe_title;

    fs::path output_dir = "output";
    fs::create_directories(output_dir);

    // video 1: instrumental
    fs::path out_vid_inst = output_dir / (safe_title + " (instrumental).mp4");
    std::cout << "rendering instrumental video..." << std::endl;
    tools.render_video(final_audio_path, p_subtitles_ass, out_vid_inst);

    // video 2 : original audio
    fs::path out_vid_orig = output_dir / (safe_title + " (original).mp4");
    std::cout << "rendering original video..." << std::endl;
    tools.render_video(p_source_wav, p_subtitles_ass, out_vid_orig);

    return 0;
}