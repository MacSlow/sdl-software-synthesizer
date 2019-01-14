#include <iostream>
#include <chrono>
#include <sstream>
#include <memory>
#include <mutex>
#include <complex>
#include "application.h"

using namespace std;
using namespace std::chrono;

#define WIN_TITLE "Audio software synthesizer by MacSlow"
#define newline '\n'

#define NOTE_C   40
#define NOTE_CIS 41
#define NOTE_D   42
#define NOTE_DIS 43
#define NOTE_E   44
#define NOTE_F   45
#define NOTE_FIS 46
#define NOTE_G   47
#define NOTE_GIS 48
#define NOTE_A   49
#define NOTE_AIS 50
#define NOTE_B   51
#define NOTE_C2  52

std::mutex synthDataMutex;

float keyToPitch (int key)
{
    // using A4 with 440Hz as the base, key = 1 is an A0
    float fkey = static_cast<float> (key);
    float pitch = pow(2.f, (fkey - 49.f)/12.f)*440.f;
    return pitch;
}

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
        float lfo = .00035f*frequency*sin (w(17.5f)*timeInSeconds);
        result += amplitude*sin (w(frequency)*timeInSeconds + lfo);
        amplitude *= .5f;
        harmonic += even ? 1.f : 2.f;
    }

    return result;
}

float oscCosine (float freq, float timeInSeconds)
{
    return cos (2.f*freq*timeInSeconds);
}

float oscNoise ()
{
    return (float) random() / (float) RAND_MAX;
}

float oscSawtooth (float freq, float timeInSeconds)
{
    float cot = oscCosine (freq, timeInSeconds) / oscSine (freq, timeInSeconds);
    float f = -atan (cot);
    return f;
}

float oscSquare (float freq, float timeInSeconds)
{
	return oscSine (freq, timeInSeconds, 24, false);
}

float oscTriangle (float freq, float timeInSeconds)
{
	return oscSine (freq, timeInSeconds, 12, true);
}

auto start = high_resolution_clock::now ();

void fillSampleBuffer (void* userdata, Uint8* stream, int lengthInBytes)
{
    std::lock_guard<std::mutex> guard(synthDataMutex);

    SynthData* synthData = reinterpret_cast<SynthData*> (userdata);
    float secondPerTick = 1.f/static_cast<float> (synthData->sampleRate);
    float volume = synthData->volume;
    float timeInSeconds = .0f;
    SDL_memset (stream, 0, lengthInBytes);
    int sizePerSample = static_cast<int> (sizeof (float));

    float* sampleBuffer = reinterpret_cast<float*> (stream);
	for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
		timeInSeconds = static_cast<float> (synthData->ticks + i/2) * secondPerTick;
        sampleBuffer[i] = .0f;
        sampleBuffer[i+1] = .0f;
        for (auto note : *synthData->notes) {
            sampleBuffer[i] += oscSine (keyToPitch (note),
                                        timeInSeconds,
                                        12,
                                        true);
            sampleBuffer[i+1] += oscSine (keyToPitch (note),
                                          timeInSeconds,
                                          24,
                                          false);
        }
        sampleBuffer[i] *= volume;
        sampleBuffer[i+1] *= volume;
	}
    synthData->ticks += ((lengthInBytes/sizePerSample - 1) / 2) + 1;
}

Application::Application (size_t width, size_t height)
    : _initialized {false}
    , _window {nullptr}
    , _running {false}
    , _notes {std::make_shared<std::list<int>> ()}
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

    _synthData.sampleRate = _sampleRate;
    _synthData.ticks = 0;
    _synthData.volume = .0f;
    _synthData.notes = _notes;

    SDL_zero (want);
    want.freq = _sampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = _channels;
    want.silence = 0;
    want.samples = _sampleBufferSize;
    want.callback = fillSampleBuffer;
    want.userdata = &_synthData;

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
    auto removeNote = [&] (int noteId) {
        for (auto it = _notes->begin();
                  it != _notes->end();) {
            if (*it == noteId) {
                it = _notes->erase (it);
            } else {
                ++it;
            }
        }
        _synthData.volume = _notes->size() == 0 ? .0f : .125f;
    };

    auto checkForNote = [&] (int noteId) {
        for (auto it = _notes->begin();
                  it != _notes->end();
                  ++it) {
            if (*it == noteId) {
                return true;
            }
        }
        return false;
    };

    auto addNote = [&] (int noteId) {
        if (_synthData.notes->size () < _maxVoices &&
            !checkForNote (noteId)) {
            _synthData.notes->emplace_back (noteId);
            _synthData.volume = .125f;
        }
    };

    SDL_Event event;

    std::lock_guard<std::mutex> guard(synthDataMutex);
    while (SDL_PollEvent (&event)) {
        switch (event.type) {
        case SDL_KEYUP:
            switch (event.key.keysym.sym) {
                case SDLK_y: removeNote (NOTE_C); break;
                case SDLK_s: removeNote (NOTE_CIS); break;
                case SDLK_x: removeNote (NOTE_D); break;
                case SDLK_d: removeNote (NOTE_DIS); break;
                case SDLK_c: removeNote (NOTE_E); break;
                case SDLK_v: removeNote (NOTE_F); break;
                case SDLK_g: removeNote (NOTE_FIS); break;
                case SDLK_b: removeNote (NOTE_G); break;
                case SDLK_h: removeNote (NOTE_GIS); break;
                case SDLK_n: removeNote (NOTE_A); break;
                case SDLK_j: removeNote (NOTE_AIS); break;
                case SDLK_m: removeNote (NOTE_B); break;
                case SDLK_COMMA: removeNote (NOTE_C2); break;
            }
        break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: {
                    _running = false;
                    break;
                }

                case SDLK_SPACE: {
                    _mute = !_mute;
                    SDL_PauseAudioDevice (_audioDevice, _mute);
                    break;
                }

                case SDLK_y: addNote (NOTE_C); break;
                case SDLK_s: addNote (NOTE_CIS); break;
                case SDLK_x: addNote (NOTE_D); break;
                case SDLK_d: addNote (NOTE_DIS); break;
                case SDLK_c: addNote (NOTE_E); break;
                case SDLK_v: addNote (NOTE_F); break;
                case SDLK_g: addNote (NOTE_FIS); break;
                case SDLK_b: addNote (NOTE_G); break;
                case SDLK_h: addNote (NOTE_GIS); break;
                case SDLK_n: addNote (NOTE_A); break;
                case SDLK_j: addNote (NOTE_AIS); break;
                case SDLK_m: addNote (NOTE_B); break;
                case SDLK_COMMA: addNote (NOTE_C2); break;

                default: break;
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
