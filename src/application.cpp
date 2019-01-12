#include <iostream>
#include <chrono>
#include <sstream>
#include <memory>
#include <complex>
#include "application.h"

using namespace std;
using namespace std::chrono;

#define WIN_TITLE "Audio software synthesizer by MacSlow"
#define newline '\n'

#define NOTE_C 261.63f 
#define NOTE_D 293.66f 
#define NOTE_E 329.63f 
#define NOTE_F 349.23f
#define NOTE_G 392.f
#define NOTE_A 441.f
#define NOTE_B 493.88f

void Application::initialize ()
{
    if (_initialized)
        return;

    int result = 0;
    SDL_ClearError ();
    result = SDL_Init (SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS);
    if (result != 0) {
        cout << "SDL_Init() failed: " << SDL_GetError () << newline;
        _initialized = false;
        return;
    }

    _initialized = true;
}

float oscSine (float baseFrequency,
               float timeInSeconds,
               int harmonics = 1,
               bool even = true) {
    float result = .0f;
    float amplitude = 1.f;
    float harmonic = 1.f;
    auto w = [](float hertz){ return 2.f*M_PI*hertz; };

    for (int i = 0; i < harmonics; ++i) {
        float frequency = baseFrequency*harmonic;
        float lfo = .5f*frequency*sin (w(1.5f)*timeInSeconds);
        result += amplitude*sin (w(frequency)*timeInSeconds + lfo);
        amplitude *= .5f;
        harmonic += even ? 1.f : 2.f;
    }

    return result;
}

float oscCosine (float freq, float timeInSeconds) {
    return cos (2.f*freq*timeInSeconds);
}

float oscNoise () {
    return (float) random() / (float) RAND_MAX;
}

float oscSawtooth (float freq, float timeInSeconds) {
    float cot = oscCosine (freq, timeInSeconds) / oscSine (freq, timeInSeconds);
    float f = -atan (cot);
    return f;
}

float oscSquare (float freq, float timeInSeconds) {
    float f = copysign (1.f, oscSine (freq, timeInSeconds));
    return f;
}

float oscTriangle (float freq, float timeInSeconds) {
    float f = 0.63661975f * asin(oscSine (freq, timeInSeconds));
    return f;
}

auto start = high_resolution_clock::now ();

void fillSampleBuffer (void* userdata, Uint8* stream, int lengthInBytes)
{
    int* sample = reinterpret_cast<int*> (userdata);
    float secondPerTick = 1.f/static_cast<float> (48000);
    float timeInSeconds = .0f;
    SDL_memset (stream, 0, lengthInBytes);
    int sizePerSample = static_cast<int> (sizeof (float));

    float* sampleBuffer = reinterpret_cast<float*> (stream);
	for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
		timeInSeconds = static_cast<float> (*sample + i/2) * secondPerTick;
        sampleBuffer[i] = .125f*oscSine (NOTE_C, timeInSeconds, 12, true);
        sampleBuffer[i] += .125f*oscSine (NOTE_D, timeInSeconds, 12, true);
        sampleBuffer[i] += .125f*oscSine (NOTE_E, timeInSeconds, 12, true);
        sampleBuffer[i+1] = .125f*oscSine (NOTE_D, timeInSeconds, 24, false);
        sampleBuffer[i+1] += .125f*oscSine (NOTE_E, timeInSeconds, 24, false);
        sampleBuffer[i+1] += .125f*oscSine (NOTE_F, timeInSeconds, 24, false);
	}
    *sample += ((lengthInBytes/sizePerSample - 1) / 2) + 1;
}

static int tick = 0;

Application::Application (size_t width, size_t height)
    : _initialized (false), _window (NULL), _running (false)
{
    initialize ();

    SDL_ClearError ();
    _window = SDL_CreateWindow (WIN_TITLE, SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, width, height, 0);
    if (!_window) {
        cout << "window creation failed: " << SDL_GetError () << newline;
        return;
    }

    int count = SDL_GetNumAudioDevices (0);
    for (int i = 0; i < count; ++i) {
        cout << "Audio device " << i
             << ": " << SDL_GetAudioDeviceName (i, 0)
             << newline;
    }

    SDL_AudioSpec want;
    SDL_AudioSpec have;

    SDL_zero (want);
    want.freq = _sampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = _channels;
    want.silence = 0;
    want.samples = _sampleBufferSize;
    want.callback = fillSampleBuffer;
    want.userdata = &tick;

    _audioDevice = SDL_OpenAudioDevice (nullptr,
                                        0,
                                        &want,
                                        &have,
                                        SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (_audioDevice == 0) {
        cout << "Failed to open audio: " << SDL_GetError() << newline;
    } else {
        if (have.format != want.format) {
            cout << "We didn't get Float32 audio format." << newline;
        }
        SDL_PauseAudioDevice (_audioDevice, _mute);
    }

    _softwareSynthesizer = make_unique<SoftwareSynthesizer> (width, height);
}

Application::~Application ()
{
    SDL_CloseAudioDevice (_audioDevice);
    SDL_DestroyWindow (_window);
    SDL_Quit ();
}

void Application::handle_events ()
{
    SDL_Event event;
    while (SDL_PollEvent (&event)) {
        switch (event.type) {
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                _running = false;
            } else if (event.key.keysym.sym == SDLK_SPACE) {
                _mute = !_mute;
                SDL_PauseAudioDevice (_audioDevice, _mute);
            }
            break;

        case SDL_QUIT:
            _running = false;
            break;
        }
    }
}

void Application::run ()
{
    if (!_initialized)
        return;

    _running = true;

    while (_running) {
        handle_events ();
        update ();
    }
}

void Application::update ()
{
    if (!_initialized)
        return;

    _softwareSynthesizer->update ();
    _softwareSynthesizer->paint (_window);
}
