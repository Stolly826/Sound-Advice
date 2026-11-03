Drop `.ttf` / `.otf` font files here, then re-run CMake. They get embedded
into the app, so they work on every teammate's machine.

Free fonts (OFL licensed, safe to ship): https://fonts.google.com

Then in code, use the font's *family name* (not the file name):

    SoundAdviceLookAndFeel::useFont (myButton, "Bungee");
