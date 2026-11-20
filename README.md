# Sound-Advice
Personal DJ software

## Playback and transcription

The compact player stays visible at the bottom of the window while navigating between pages. It shows the current track and playback progress, and lets you pause, resume, or seek.

Song transcription runs locally through whisper.cpp; audio is not uploaded. The first time you choose **Transcribe audio**, select a compatible Whisper GGML `.bin` model file. Sound Advice remembers that model path in the user's application data folder. The model is not bundled with the app.

Transcripts are scanned for the English terms shown by **Explicit word list** on a playlist. Matching songs are labeled **EXPLICIT**, and **Hide explicit** filters them out of the playlist and its navigation sidebar.
