
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

#building soundadvice
add_executable(SoundAdviceMock
    SoundAdviceMock.cpp
    SoundAdviceMock.h
)

#setting config rules for C++ code to be compatible w JUCE (looked this up)
target_compile_features(SoundAdviceMock PRIVATE cxx_std_17)

target_compile_definitions(SoundAdviceMock PRIVATE
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
)

#connecting JUCE's audio lib
target_link_libraries(SoundAdviceMock PRIVATE
    juce::juce_audio_formats
    juce::juce_audio_devices
    comdlg32
)
