# Foobar2k Steam Audio DSP

#### ⚠️ I'm just dumping the code, I'm not actively working on it ⚠️
#### ⚠️ It's also not very stable ⚠️

This DSP applies Steam Audio processing to the audio.
It is meant to be used with multi channel streams as the different channels are positioned as virtual speakers inside the space.

##### Installing

Download the latest foo_steamaudio.dll from the [releases tab](https://github.com/djpadbit/foo_steamaudio/releases) and the [Steam Audio 4.2.0 SDK](https://github.com/ValveSoftware/steam-audio/releases/download/v4.2.0/steamaudio_4.2.0.zip). Grab phonon.dll from steamaudio/lib/windows-x86 and put it next to foo_steamaudio.dll in a folder called foo_steamaudio in your user component folder of foobar2k.

You'll figure it out, I belive in you :)

##### Building

This is based on the foo_sample example component, so it compiles the same way.
It was made for the foobar2k SDK2022-01-04 and Steam Audio 4.2.0. I built it using Visual Studio 2022

You need to download the steam audio SDK and extract it into the steamaudio/ folder to build (you should have steamaudio/include/...)
Then you need to put the steamaudio dll next to the DSP dll for foobar2k to load it properly.

Again, you'll figure it out, I still belive in you.