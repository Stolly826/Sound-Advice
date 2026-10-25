#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_events/juce_events.h>
#include <memory>

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

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(
            reader.release(), true
        );

        transport.setSource(readerSource.get(), 0, nullptr, sampleRate);
        return true;
    }

    bool play()
    {
        if (!readerSource)
            return false;

        if (transport.hasStreamFinished())
            transport.setPosition(0.0);

        transport.start(); //starts playing the selected song
        return true;
    }

    void stop()
    {
        transport.stop(); //stops playback
        transport.setPosition(0.0); //moves the song back to the beginning
    }

private:
    juce::AudioFormatManager formats;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::AudioSourcePlayer player;
    juce::AudioDeviceManager deviceManager;
    juce::String error;
};

AudioEngine* audio = nullptr;
HWND fileLabel;

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

        fileLabel = CreateWindowW(L"STATIC", L"No audio selected",
            WS_VISIBLE | WS_CHILD,
            30, 150, 700, 30,
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
    return 0;
}
