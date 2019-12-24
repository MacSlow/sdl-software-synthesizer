#include <algorithm>
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

static float elapsedSeconds ()
{
    return static_cast<float>(SDL_GetTicks())*.001;
}

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

float w(float hertz)
{
    return 2.f*M_PI*hertz;
}

float oscSine (float baseFrequency,
               float timeInSeconds,
               int harmonics = 1,
               bool even = true) {
    float result = .0f;
    float amplitude = 1.f;
    float harmonic = 1.f;

    for (int i = 0; i < harmonics; ++i) {
        float frequency = baseFrequency*harmonic;
        result += amplitude*sin (w(frequency)*timeInSeconds);
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

float oscSawtooth (float freq, float timeInSeconds, float steps = 10.f)
{
    float result = .0f;
    float scale = 2.f/M_PI;
    for (float i = 1.f; i < steps; i += 1.f) {
        result += (1.f/i) * sin (i*M_PI*freq*timeInSeconds);
    }
    return scale*result;
}

float oscSquare (float freq, float timeInSeconds, float steps = 10.f)
{
    float result = .0f;
    float scale =  4.f/M_PI;
    float numerator = .0f;
    float denominator = .0f;
    for (float i = 1.f; i < steps; i += 1.f) {
        numerator = sin (2.f*M_PI*(2.f*i - 1.f)*freq*timeInSeconds);
        denominator = 2.f*i - 1.f;
        result += numerator/denominator;
    }
	return scale*result;
}

float oscTriangle (float freq, float timeInSeconds)
{
	return oscSine (freq, timeInSeconds, 12, true);
}

void fillSampleBuffer (void* userdata, Uint8* stream, int lengthInBytes)
{
    std::lock_guard<std::mutex> guard(synthDataMutex);

    SynthData* synthData = reinterpret_cast<SynthData*> (userdata);
    float secondPerTick = 1.f/static_cast<float> (synthData->sampleRate);
    float volume = .2f; //synthData->volume;
    float timeInSeconds = .0f;
    SDL_memset (stream, 0, lengthInBytes);
    int sizePerSample = static_cast<int> (sizeof (float));

    float* sampleBuffer = reinterpret_cast<float*> (stream);
	for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
		timeInSeconds = static_cast<float> (synthData->ticks + i/2) * secondPerTick;
        sampleBuffer[i] = .0f;
        sampleBuffer[i+1] = .0f;

        for (auto note : *synthData->notes) {
            float level = note.envelope.level (elapsedSeconds());
            sampleBuffer[i] += oscSine (keyToPitch (note.noteId),
                                        timeInSeconds,
                                        12,
                                        true);
            sampleBuffer[i+1] += oscSine (keyToPitch (note.noteId),
                                          timeInSeconds,
                                          24,
                                          false);

            sampleBuffer[i] *= level;
            sampleBuffer[i+1] *= level;
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
    , _synth {Synth(_maxVoices)}
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
    _synthData.notes = _synth.notes();

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
    SDL_Event event;

    auto addNote = [=](SDL_Keycode key, NoteId noteId) {
        if (!_pressedKeys[key]) {
            _synth.addNote (noteId);
            _pressedKeys[key] = true;
        }
    };

    auto removeNote = [=](SDL_Keycode key, NoteId noteId) {
        _synth.removeNote (noteId);
        _pressedKeys[key] = false;
    };

    std::lock_guard<std::mutex> guard(synthDataMutex);
    while (SDL_PollEvent (&event)) {
        switch (event.type) {
        case SDL_KEYUP:
            switch (event.key.keysym.sym) {
                case SDLK_y: removeNote (SDLK_y, NOTE_C); break;
                case SDLK_s: removeNote (SDLK_s, NOTE_CIS); break;
                case SDLK_x: removeNote (SDLK_x, NOTE_D); break;
                case SDLK_d: removeNote (SDLK_d, NOTE_DIS); break;
                case SDLK_c: removeNote (SDLK_c, NOTE_E); break;
                case SDLK_v: removeNote (SDLK_v, NOTE_F); break;
                case SDLK_g: removeNote (SDLK_g, NOTE_FIS); break;
                case SDLK_b: removeNote (SDLK_b, NOTE_G); break;
                case SDLK_h: removeNote (SDLK_h, NOTE_GIS); break;
                case SDLK_n: removeNote (SDLK_n, NOTE_A); break;
                case SDLK_j: removeNote (SDLK_j, NOTE_AIS); break;
                case SDLK_m: removeNote (SDLK_m, NOTE_B); break;
                case SDLK_COMMA: removeNote (SDLK_COMMA, NOTE_C2); break;
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

                // case SDLK_y: _synth.addNote (NOTE_C); _pressedKeys[SDLK_y] = true; break;
                case SDLK_y: addNote (SDLK_y, NOTE_C); break;
                case SDLK_s: addNote (SDLK_s, NOTE_CIS); break;
                case SDLK_x: addNote (SDLK_x, NOTE_D); break;
                case SDLK_d: addNote (SDLK_d, NOTE_DIS); break;
                case SDLK_c: addNote (SDLK_c, NOTE_E); break;
                case SDLK_v: addNote (SDLK_v, NOTE_F); break;
                case SDLK_g: addNote (SDLK_g, NOTE_FIS); break;
                case SDLK_b: addNote (SDLK_b, NOTE_G); break;
                case SDLK_h: addNote (SDLK_h, NOTE_GIS); break;
                case SDLK_n: addNote (SDLK_n, NOTE_A); break;
                case SDLK_j: addNote (SDLK_j, NOTE_AIS); break;
                case SDLK_m: addNote (SDLK_m, NOTE_B); break;
                case SDLK_COMMA: addNote (SDLK_COMMA, NOTE_C2); break;

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
         _synth.clearNotes ();
    }
}

void Application::update ()
{
    if (!_initialized)
        return;

    _softwareSynthesizer->update ();
    _softwareSynthesizer->paint (_window);
}

Synth::Synth (unsigned int maxVoices)
    : _notes {std::make_shared<Notes>()}
    , _maxVoices {maxVoices}
{
}

void Synth::addNote (NoteId noteId)
{
    if (_notes->size() < _maxVoices) {
        auto result = find_if (_notes->begin(),
                               _notes->end(),
                               [noteId] (Note note) {
                                   return note.noteId == noteId;
                               });
        if (result != _notes->end()) {
            if ((*result).envelope.noteReleased) {
                (*result).envelope.noteOn (elapsedSeconds());
                (*result).envelope.noteOff (.0f);
            }
        } else {
            Note note;
            note.noteId = noteId;
            note.envelope.noteOn (elapsedSeconds());
            _notes->emplace_back (note);
        }
    }
}

void Synth::removeNote (NoteId noteId)
{

    auto result = find_if (_notes->begin(),
                           _notes->end(),
                           [noteId] (Note note) {
                               return note.noteId == noteId;
                           });

    if (result != _notes->end()) {
        (*result).envelope.noteOff (elapsedSeconds());
    }
}

void Synth::clearNotes ()
{
    std::lock_guard<std::mutex> guard(synthDataMutex);
    _notes->remove_if([](Note& note){ return note.envelope.level(elapsedSeconds()) < .0001f; });
}

std::shared_ptr<Notes> Synth::notes()
{
    return _notes;
}
