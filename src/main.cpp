#include <iostream>
#include <ExternalTools.hpp>
#include <LyricsEngine.hpp>

int main() {

    std::cout << "--- pipeline component test ---" << std::endl;

    ExternalTools tools;

    // 1. test download

    std::cout << "\n[step 1] downloading audio..." << std::endl;

    auto audio_path = tools.download_audio("https://www.youtube.com/watch?v=dQw4w9WgXcQ");

    if (!audio_path) return 1;
    std::cout << "[success] downloaded to: " << audio_path->string() << std::endl;

    // 2. skip separator for now

    std::cout << "\n [step 2] skipping separator..." << std::endl;
    auto instrumental_path = *audio_path;

    // 3. dummy subtitles
    std::cout << "\n [step 3] generating subtitles..." << std::endl;
    AssConverter converter;
    std::vector<LyricLine> lines;

    LyricLine l1; l1.start_time = 0.0; l1.end_time = 5.0; l1.text = "Never gonna give you up";
    LyricLine l2; l2.start_time = 5.0; l2.end_time = 10.0; l2.text = "Never gonna let you down";
    lines.push_back(l1);
    lines.push_back(l2);

    std::string ass_content = converter.generate_ass(lines);
    std::filesystem::path ass_path = "temp/test.ass";
    converter.save_to_file(ass_content, ass_path);

    std::cout << "\n rendering video..." << std::endl;
    
    if (tools.render_video(instrumental_path, ass_path, "rick_test.mp4")) {
        std::cout << "[success] pipeline finished" << std::endl;
    } else {
        std::cerr << "[error] rendering failed" << std::endl;
    }

    return 0;
}