
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <string>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <cwctype>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")

HWND fileLabel;
std::wstring selectedFile;

const std::uintmax_t MAX_WAV_SIZE = 100ull * 1024 * 1024; //100 MB upload limit
const std::uint16_t FORMAT_PCM = 1;
const std::uint16_t FORMAT_EXTENSIBLE = 0xFFFE;

//checks that the file is a real wav file that PlaySound can play, error gets filled in with the reason if it isnt
bool ValidateWav(const std::wstring& path, std::wstring& error)
{
    //extension check, case insensitive so .WAV works too
    if (path.size() < 4)
    {
        error = L"File must be a .wav file.";
        return false;
    }
    std::wstring ext = path.substr(path.size() - 4);
    for (auto& c : ext)
        c = std::towlower(c);
    if (ext != L".wav")
    {
        error = L"File must be a .wav file.";
        return false;
    }

    //make sure the file is still there and isnt empty or too big
    std::error_code ec;
    std::filesystem::path fsPath(path);
    if (!std::filesystem::is_regular_file(fsPath, ec))
    {
        error = L"File could not be found.";
        return false;
    }
    std::uintmax_t size = std::filesystem::file_size(fsPath, ec);
    if (ec || size < 44) //44 bytes is the smallest a wav header can be
    {
        error = L"File is empty or too small to be a valid WAV file.";
        return false;
    }
    if (size > MAX_WAV_SIZE)
    {
        error = L"File is too large. The limit is 100 MB.";
        return false;
    }

    std::ifstream file(fsPath, std::ios::binary);
    if (!file)
    {
        error = L"File could not be opened.";
        return false;
    }

    //every wav file starts with "RIFF" <size> "WAVE", catches renamed mp3s and other files
    char header[12];
    if (!file.read(header, 12) || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0)
    {
        error = L"File is not a valid WAV file.";
        return false;
    }

    //walk through the chunks to find the fmt (audio format) and data (actual audio) chunks
    bool foundFmt = false;
    bool foundData = false;
    char chunkId[4];
    std::uint32_t chunkSize = 0;
    while (file.read(chunkId, 4) && file.read(reinterpret_cast<char*>(&chunkSize), 4))
    {
        std::streampos chunkStart = file.tellg();

        if (std::memcmp(chunkId, "fmt ", 4) == 0)
        {
            if (chunkSize < 16)
            {
                error = L"WAV file has a corrupted format header.";
                return false;
            }
            std::uint16_t formatTag = 0, channels = 0, bitsPerSample = 0;
            std::uint32_t sampleRate = 0;
            file.read(reinterpret_cast<char*>(&formatTag), 2);
            file.read(reinterpret_cast<char*>(&channels), 2);
            file.read(reinterpret_cast<char*>(&sampleRate), 4);
            file.seekg(6, std::ios::cur); //skip byte rate and block align
            file.read(reinterpret_cast<char*>(&bitsPerSample), 2);

            //extensible format stores the real format in the first 2 bytes of the subformat guid
            if (formatTag == FORMAT_EXTENSIBLE && chunkSize >= 40)
            {
                file.seekg(8, std::ios::cur); //skip cbSize, valid bits and channel mask
                file.read(reinterpret_cast<char*>(&formatTag), 2);
            }

            if (!file)
            {
                error = L"WAV file has a corrupted format header.";
                return false;
            }
            if (formatTag != FORMAT_PCM)
            {
                error = L"Only uncompressed PCM WAV files are supported.";
                return false;
            }
            if (channels == 0 || sampleRate == 0 || bitsPerSample == 0)
            {
                error = L"WAV file has invalid audio settings.";
                return false;
            }
            foundFmt = true;
        }
        else if (std::memcmp(chunkId, "data", 4) == 0)
        {
            if (chunkSize == 0)
            {
                error = L"WAV file contains no audio.";
                return false;
            }
            foundData = true;
        }

        if (foundFmt && foundData)
            return true;

        //chunks are padded to an even number of bytes
        file.clear();
        file.seekg(chunkStart + static_cast<std::streamoff>(chunkSize + (chunkSize & 1)));
    }

    error = foundFmt ? L"WAV file contains no audio." : L"WAV file is missing its format information.";
    return false;
}

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
            ofn.lpstrFilter = L"WAV Files\0*.wav\0All Files\0*.*\0";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (!GetOpenFileNameW(&ofn))
            {
                //returns 0 when the user just hit cancel, anything else is an actual error
                DWORD dialogError = CommDlgExtendedError();
                if (dialogError == FNERR_BUFFERTOOSMALL)
                    MessageBoxW(hwnd, L"The file path is too long.", L"Upload failed", MB_OK | MB_ICONWARNING);
                else if (dialogError != 0)
                    MessageBoxW(hwnd, L"The file dialog could not be opened.", L"Upload failed", MB_OK | MB_ICONWARNING);
                return 0;
            }

            std::wstring error;
            if (!ValidateWav(filename, error)) //keeps the previous selection if the new file is rejected
            {
                MessageBoxW(hwnd, error.c_str(), L"Upload failed", MB_OK | MB_ICONWARNING);
                return 0;
            }

            selectedFile = filename;
            SetWindowTextW(fileLabel, selectedFile.c_str());
            return 0;
        }

        case 2:
        {
            if (selectedFile.empty())
            {
                MessageBoxW(hwnd, L"Upload a WAV file first.", L"Nothing to play", MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            //recheck in case the file was moved, deleted or changed since it was uploaded
            std::wstring error;
            if (!ValidateWav(selectedFile, error))
            {
                MessageBoxW(hwnd, error.c_str(), L"Playback failed", MB_OK | MB_ICONWARNING);
                selectedFile.clear();
                SetWindowTextW(fileLabel, L"No audio selected");
                return 0;
            }

            //SND_NODEFAULT stops windows from playing the default ding if playback fails
            if (!PlaySoundW(selectedFile.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT))
                MessageBoxW(hwnd, L"The file could not be played.", L"Playback failed", MB_OK | MB_ICONWARNING);
            return 0;
        }

        case 3:
            PlaySoundW(nullptr, nullptr, 0);
            return 0;
        }
        break;

    case WM_DESTROY:
        PlaySoundW(nullptr, nullptr, 0);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int main()
{
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

    MSG msg = {}; //message loop, created variable msg and then waits for windows to send a msg then stores it in msg
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) 
    {
        TranslateMessage(&msg); //process keyboard inputs 
        DispatchMessageW(&msg); //sends keyboard input to the write spot
    }

    return 0;
}

