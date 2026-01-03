cmake_minimum_required(VERSION 3.10)
project(karaoke_cpp)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(src)

add_executable(karaoke
    src/main.cpp
    src/LyricsEngine.cpp
    src/ExternalTools.cpp
)

target_link_libraries(karaoke
    CURL::libcurl
    nlohmann_json::nlohmann_json
)
