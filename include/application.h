#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <SDL.h>
#include <memory>

#include "software-synthesizer.h"

struct SynthData
{
	float sampleRate;
	int ticks;
	float note;
	float volume;
};

class Application {
    public:
        Application (size_t width, size_t height);
        ~Application ();

        void run ();
        void update ();

    private:
        void initialize ();
        void handle_events ();

    private:
        bool _initialized = false;
        SDL_Window* _window = nullptr;
        bool _running = true;
        std::unique_ptr<SoftwareSynthesizer> _softwareSynthesizer;
        SDL_AudioDeviceID _audioDevice;
        bool _mute = false;
        int _sampleRate = 48000;
        int _channels = 2;
        int _sampleBufferSize = 512;
        SynthData _synthData;
};

#endif // _APPLICATION_H
