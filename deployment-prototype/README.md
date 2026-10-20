Basic C++ application using CMake to test
Windows executable creation and audio playback.

Currently supports WAV audio files only because thats what is already integrated into windows but I will continue to add integration when the JUCE stuff is ready
its also mainly in window deployment for now but when i get all the code and stuff i can add the .exe stuff too

## TagLib (audio metadata)

TagLib v2.3.2 is pulled in automatically by CMake (`FetchContent`) and built as a static library the first time you configure, so there's nothing to install (you do need git and an internet connection for that first configure).

It isn't used by the app yet. When you need it, include `AudioMetadata.h` and call:

```cpp
#include "AudioMetadata.h"

if (auto meta = readAudioMetadata(selectedFile)) // works with std::wstring paths
{
    // meta->title, meta->artist, meta->album, meta->lengthMs, meta->sampleRate, ...
}
```

For anything beyond the basics, use TagLib directly (`#include <fileref.h>`, `TagLib::FileRef`, etc.); see https://taglib.org/api/.
