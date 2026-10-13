
#include <windows.h>
#include <commdlg.h>
#include <mmsystem.h>
#include <string>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")

HWND fileLabel;
std::wstring selectedFile;

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

            if (GetOpenFileNameW(&ofn))
            {
                selectedFile = filename;
                SetWindowTextW(fileLabel, selectedFile.c_str());
            }
            return 0;
        }

        case 2:
            if (!selectedFile.empty())
            {
                PlaySoundW(selectedFile.c_str(), nullptr,
                    SND_FILENAME | SND_ASYNC);
            }
            return 0;

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

