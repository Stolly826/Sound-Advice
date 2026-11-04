
cmake_minimum_required(VERSION 3.22)

project(SoundAdviceMock LANGUAGES C CXX)

#downloading JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

#downloading TagLib (reads song title, artist, bpm etc from the file tags)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BINDINGS OFF CACHE BOOL "" FORCE)
set(WITH_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    taglib
    GIT_REPOSITORY https://github.com/taglib/taglib.git
    GIT_TAG v2.0.2
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(taglib)

#building soundadvice
add_executable(SoundAdviceMock
    SoundAdviceMock.cpp
    SoundAdviceMock.h
    SongData.h
)

#setting config rules for C++ code to be compatible w JUCE (looked this up)
target_compile_features(SoundAdviceMock PRIVATE cxx_std_17)

target_compile_definitions(SoundAdviceMock PRIVATE
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    TAGLIB_STATIC
)

#TagLib only exports its include folders when installed so we point at them directly
target_include_directories(SoundAdviceMock PRIVATE
    ${taglib_SOURCE_DIR}/taglib
    ${taglib_SOURCE_DIR}/taglib/toolkit
)

#connecting JUCE's audio lib
target_link_libraries(SoundAdviceMock PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_devices
    tag
    comdlg32
    comctl32 #Windows slider (trackbar) control
)
