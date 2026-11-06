cmake_minimum_required(VERSION 3.22)

project(SoundAdvice VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
# Change the tag here if the team moves versions (PR #3 uses 9.0.4).
# Or override without editing:  cmake -B build -DSOUNDADVICE_JUCE_TAG=9.0.4
set(SOUNDADVICE_JUCE_TAG "9.0.3" CACHE STRING "JUCE git tag to download")

# Already have JUCE cloned somewhere? Skip the download with:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/path/to/JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${SOUNDADVICE_JUCE_TAG}
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# The app
# ---------------------------------------------------------------------------
# juce_add_gui_app (not plain add_executable) gives us a real Windows GUI exe
# and a proper .app bundle on macOS.
juce_add_gui_app(SoundAdvice
    PRODUCT_NAME  "Sound Advice"
    COMPANY_NAME  "Sound Advice Team"
    BUNDLE_ID     "com.soundadvice.app"
    VERSION       ${PROJECT_VERSION}
    # ICON_BIG    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"   # add later
)

# Add new .cpp files here as the frontend grows.
target_sources(SoundAdvice PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/MainComponent.h
    Source/AudioEngine.cpp
    Source/AudioEngine.h
    Source/Sidebar.cpp
    Source/Sidebar.h
    Source/SoundAdviceLookAndFeel.cpp
    Source/SoundAdviceLookAndFeel.h
    Source/Pages/AddSongDialog.cpp
    Source/Pages/AddSongDialog.h
    Source/Pages/PickSongDialog.cpp
    Source/Pages/PickSongDialog.h
    Source/Pages/SaveTransitionDialog.cpp
    Source/Pages/SaveTransitionDialog.h
    Source/Pages/Transition.h
    Source/Pages/MainMenuPage.cpp
    Source/Pages/MainMenuPage.h
    Source/Pages/NewPlaylistDialog.cpp
    Source/Pages/NewPlaylistDialog.h
    Source/Pages/NewPlaylistRow.cpp
    Source/Pages/NewPlaylistRow.h
    Source/Pages/Playlist.h
    Source/Pages/PlaylistDetailPage.cpp
    Source/Pages/PlaylistDetailPage.h
    Source/Pages/PlaylistRow.cpp
    Source/Pages/PlaylistRow.h
    Source/Pages/PlaylistsPage.cpp
    Source/Pages/PlaylistsPage.h
    Source/Pages/PlaylistsSidebarContent.cpp
    Source/Pages/PlaylistsSidebarContent.h
    Source/Pages/MixingPage.cpp
    Source/Pages/MixingPage.h
    Source/Pages/SongChartComponent.cpp
    Source/Pages/SongChartComponent.h
    Source/Pages/Song.h
    Source/Pages/SongDetailDialog.cpp
    Source/Pages/SongDetailDialog.h
    Source/Pages/SongRow.cpp
    Source/Pages/SongRow.h
    Source/Pages/SongsSidebarContent.cpp
    Source/Pages/SongsSidebarContent.h
    Source/Pages/SongsPage.cpp
    Source/Pages/SongsPage.h
    Source/Pages/TransitionsPage.cpp
    Source/Pages/TransitionsPage.h
)

# Custom fonts: every .ttf/.otf in Resources/Fonts gets embedded into the exe.
# Re-run CMake after adding or removing a font file.
file(GLOB SOUNDADVICE_FONT_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts/*.otf"
)

if(SOUNDADVICE_FONT_FILES)
    juce_add_binary_data(SoundAdviceFonts
        HEADER_NAME FontData.h
        NAMESPACE   FontData
        SOURCES     ${SOUNDADVICE_FONT_FILES}
    )
    target_link_libraries(SoundAdvice PRIVATE SoundAdviceFonts)
    target_compile_definitions(SoundAdvice PRIVATE SOUNDADVICE_HAS_FONTS=1)
endif()

target_compile_features(SoundAdvice PRIVATE cxx_std_17)

target_compile_definitions(SoundAdvice PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SoundAdvice,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SoundAdvice,JUCE_VERSION>"
    JUCE_USE_MP3AUDIOFORMAT=1     # play MP3s, not just WAV
    JUCE_USE_CURL=0               # we don't need networking
    JUCE_WEB_BROWSER=0            # no embedded browser
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(SoundAdvice
    PRIVATE
        juce::juce_gui_basics      # windows, buttons, sliders, layout
        juce::juce_gui_extra
        juce::juce_audio_basics
        juce::juce_audio_formats   # reading WAV / MP3
        juce::juce_audio_devices   # speakers / Core Audio / WASAPI
        juce::juce_audio_utils     # waveform thumbnails, device selector UI
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
