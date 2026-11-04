#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <commctrl.h> //provides the Win32 slider controls and slider messages
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <memory>
#include <string>
#include "SongData.h"

#pragma comment(lib, "comdlg32.lib")
//updated commands with help of claude code btw im not goated like that
//JUCE audio engine this is replacing PlaySoundW so we can play WAV and MP3 files
class AudioEngine
{
public:
    AudioEngine()
    {
        formats.registerBasicFormats(); //tells JUCE which audio formats it can open

        error = deviceManager.initialiseWithDefaultDevices(0, 2); //connects to the default speakers
        if (error.isEmpty())
        {
            player.setSource(&transport); //connects the playback controls to the audio output
            deviceManager.addAudioCallback(&player);
        }
    }

    ~AudioEngine()
    {
        deviceManager.removeAudioCallback(&player);
        player.setSource(nullptr);
        transport.stop();
        transport.setSource(nullptr);
        readerSource.reset();
        deviceManager.closeAudioDevice();
    }

    bool isReady() const
    {
        return error.isEmpty();
    }

    const juce::String& getError() const
    {
        return error;
    }

    bool loadFile(const wchar_t* path)
    {
        //JUCE reads the audio file instead of using the old Windows PlaySoundW function
        std::unique_ptr<juce::AudioFormatReader> reader(
            formats.createReaderFor(juce::File(juce::String(path)))
        );

        if (!reader)
            return false;

        double sampleRate = reader->sampleRate;

        transport.stop();
        transport.setSource(nullptr);
        paused = false; //variable for tracking pause or not so resume doesn't resume a song that was never paused, also resets when new song is uploaded

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            reader.release(), true
        );

        transport.setSource(readerSource.get(), 0, nullptr, sampleRate);
        return true;
    }

    juce::AudioFormatManager& getFormats()
    {
        return formats;
    }

    bool play()
    {
        if (!readerSource)
            return false;

        if (transport.hasStreamFinished())
            transport.setPosition(0.0);

        transport.start(); //starts playing song so paused is now false again
        paused = false;
        return true;
    }


    bool pause()
    {
        //only pause when a song is loaded and currently playing
        if (!readerSource || !transport.isPlaying())
            return false;

        transport.stop(); //keep position of current song
        paused = true;
        return true;
    }

    bool resume()
    {
        if (!readerSource || !paused) //making sure the song is paused before resuming
            return false;

        transport.start(); //continue
        paused = false;
        return true;
    }

    void setVolume(float level)
    {
        transport.setGain(level); //assigned gain value for JUCE so 0 is mute, 0.5 is hald and 1 is full
    }

    void stop()
    {
        transport.stop();
        transport.setPosition(0.0); //reset song
        paused = false;
    }

private:
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer player;
    juce::AudioDeviceManager deviceManager;
    juce::String error;
    //tracking if the song was paused or stopped
    bool paused = false;
};

AudioEngine* audio = nullptr;
SongLibrary* library = nullptr;
HWND fileLabel;
HWND detailsLabel;
HWND volumeSlider; //volume slkider stuff
HWND volumeLabel;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateWindowW(L"BUTTON", L"Upload Audio",
            WS_VISIBLE | WS_CHILD, //visible to make it immediately pop up and child to make it known its in teh window
            30, 30, 150, 40, //dimensions and placement for buttons
            hwnd, (HMENU)1, nullptr, nullptr); //identified parent window with hwnd and HMENU# assigns the button an id to recognize which button was clicked

        CreateWindowW(L"BUTTON", L"Play",
            WS_VISIBLE | WS_CHILD,
            30, 90, 100, 40,
            hwnd, (HMENU)2, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Stop",
            WS_VISIBLE | WS_CHILD,
            150, 90, 100, 40,
            hwnd, (HMENU)3, nullptr, nullptr);

        CreateWindowW(L"BUTTON", L"Pause",
            WS_VISIBLE | WS_CHILD,
            270, 90, 100, 40,
            hwnd, (HMENU)4, nullptr, nullptr); //button 4 is pause

        CreateWindowW(L"BUTTON", L"Resume",
            WS_VISIBLE | WS_CHILD,
            390, 90, 100, 40, //making sure resume and pause are next toi each other
            hwnd, (HMENU)5, nullptr, nullptr); //button 5 is resume

        volumeLabel = CreateWindowW(L"STATIC", L"Volume: 100%",
            WS_VISIBLE | WS_CHILD,
            30, 155, 150, 25,
            hwnd, nullptr, nullptr, nullptr); //show %

        volumeSlider = CreateWindowW(TRACKBAR_CLASSW, L"",
            WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_NOTICKS,
            30, 180, 300, 35,
            hwnd, (HMENU)6, nullptr, nullptr); //6 is volume slider

        SendMessageW(volumeSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(volumeSlider, TBM_SETPOS, TRUE, 100); 
        audio->setVolume(1.0f); //match whatever is JUCE's expected volume or base setting to the slider

        fileLabel = CreateWindowW(L"STATIC", L"No audio selected",
            WS_VISIBLE | WS_CHILD,
            30, 235, 700, 30,
            hwnd, nullptr, nullptr, nullptr);

        detailsLabel = CreateWindowW(L"STATIC", L"",
            WS_VISIBLE | WS_CHILD,
            30, 275, 700, 200, //tall enough for one line per song detail
            hwnd, nullptr, nullptr, nullptr);
        return 0;

    case WM_COMMAND: //used ai for this part to write me a windowsapi command function
        switch (LOWORD(wParam))
        {
        case 1:
        {
            wchar_t filename[MAX_PATH] = {};
            OPENFILENAMEW ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter =
                L"Audio Files\0*.wav;*.mp3\0"
                L"WAV Files\0*.wav\0"
                L"MP3 Files\0*.mp3\0";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileNameW(&ofn))
            {
                if (audio->loadFile(filename)) //loading the WAV or MP3 file
                {
                    SetWindowTextW(fileLabel, filename);

                    //reading the whole song for bpm and volume can take a second so show the loading cursor
                    HCURSOR oldCursor = SetCursor(LoadCursorA(nullptr, IDC_WAIT));
                    SongInfo song = SongData::readSong(audio->getFormats(), juce::File(juce::String(filename)));
                    SetCursor(oldCursor);

                    juce::String details = song.describe();
                    if (!library->save(song)) //stores the details in library.json
                        details += "\n\n(could not save to " + library->getFile().getFullPathName() + ")";
                    SetWindowTextW(detailsLabel, details.toWideCharPointer());
                }
                else
                {
                    MessageBoxW(hwnd, L"Unable to load this audio file.",
                        L"SoundAdvice", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        }

        case 2:
            if (!audio->play()) //using JUCE to handle play and stop instead of PlaySoundW
            {
                MessageBoxW(hwnd, L"Upload an audio file first.",
                    L"SoundAdvice", MB_OK);
            }
            return 0;

        case 3:
            audio->stop(); //stop
            return 0;

        case 4:
            audio->pause(); //whn you press button 4 you pause and dont reset song
            return 0;

        case 5:
            audio->resume(); //continue on with button 5
            return 0;
        }
        break;

    case WM_HSCROLL:
        if (reinterpret_cast<HWND>(lParam) == volumeSlider)
        {
            //get the current slider position an integer from 0 to 100
            int volumePercent = static_cast<int>(SendMessageW(volumeSlider, TBM_GETPOS, 0, 0));
            //concvert volume gain/control into JUCE's formatting so 0-100 is now 0-1.0
            audio->setVolume(static_cast<float>(volumePercent) / 100.0f);
            std::wstring text = L"Volume: " + std::to_wstring(volumePercent) + L"%"; //update volume when slider moves
            SetWindowTextW(volumeLabel, text.c_str());
            return 0;
        }
        break;

    case WM_DESTROY:
        audio->stop();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser; //initializes JUCE for this application
    AudioEngine engine;

    if (!engine.isReady())
    {
        MessageBoxW(nullptr, engine.getError().toWideCharPointer(),
            L"Audio initialization failed", MB_OK | MB_ICONERROR);
        return 1;
    }
    audio = &engine;

    SongLibrary songLibrary; //loads library.json from previous runs
    library = &songLibrary;

    //initialize window slider controls
    INITCOMMONCONTROLSEX controls = {};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES; //enable trackbar/common control classes
    if (!InitCommonControlsEx(&controls))
        return 1;

    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW wc = {}; //empty window class config 
    wc.lpfnWndProc = WindowProc; //connect the window to the function handling events such as closing the window and button clicks
    wc.hInstance = instance; //associating the window class with the program
    wc.lpszClassName = L"SoundAdviceWindow"; //giving the window class an internal name
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); //set baackground bush

    if (!RegisterClassW(&wc))
        return 1;

    HWND hwnd = CreateWindowExW(
        0, wc.lpszClassName,
        L"SoundAdvice Mock", //title
        WS_OVERLAPPEDWINDOW, //gives the standard window features of a title bar, allowing you to resize and minimize/maximize buttons 
        CW_USEDEFAULT, CW_USEDEFAULT, //starting position when the window opens
        900, 600, //height and width
        nullptr, nullptr, instance, nullptr
    );

    if (!hwnd) //checking if creating the window succeeded
        return 1;

    ShowWindow(hwnd, SW_SHOW);

    MSG msg = {}; //message loop created variable msg and then waits for windows to send a msg then stores it in msg
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg); //process keyboard inputs 
        DispatchMessageW(&msg); //sends keyboard input to the write spot
    }

    audio = nullptr;
    library = nullptr;
    return 0;
}
