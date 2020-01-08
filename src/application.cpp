#include <algorithm>
#include <chrono>
#include <complex>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
#include <sstream>
#include <thread>

#include "application.h"

using namespace std;
using namespace std::chrono;
using std::make_shared;

#define WIN_TITLE "12-way polyphonic synthesizer by MacSlow"
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
std::mutex midiMessageQueueMutex;

static float elapsedSeconds ()
{
    return static_cast<float>(SDL_GetTicks())*.001;
}

float keyToPitch (int key, float detune = .0f /* 0..100 */)
{
    // using A4 with 440Hz as the base, key = 1 is an A0
    float fkey = static_cast<float> (key);
    fkey += detune*.01f;
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

    _sampleBufferForDrawing.reserve(_sampleBufferSize*2);

    std::fill(_sampleBufferForDrawing.begin(),
              _sampleBufferForDrawing.end(),
              .0f);

    _initialized = true;

    std::thread midiKeyReadingThread (readMidiKeys,
                                      std::ref (_midi),
                                      std::ref(_midiMessageQueue));
    midiKeyReadingThread.detach();
}

void Application::readMidiKeys(const Midi& midi, std::queue<MessageData>& queue)
{
    while (true) {
        MessageData messageData = midi.read();

        std::lock_guard<std::mutex> guard(midiMessageQueueMutex);
        queue.push (messageData);
    }
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
    float maxHarmonics = static_cast<float>(harmonics);

    for (float i = .0f; i < maxHarmonics; (even ? i += 1.f : i += 2.f)) {
        harmonic = 1.f + i;
        amplitude = 1.f/harmonic;
        float frequency = baseFrequency*harmonic;
        result += amplitude*sin (w(frequency)*timeInSeconds);
    }

    return result;
}

float oscNoise ()
{
    return (float) random() / (float) RAND_MAX;
}

float oscSawtooth (float freq, float timeInSeconds, int harmonics = 32)
{
    return oscSine (freq, timeInSeconds, harmonics, true);
}

float oscSquare (float freq, float timeInSeconds, int harmonics = 64)
{
    return oscSine (freq, timeInSeconds, harmonics, false);
}

void fillVoiceBuffer (int instrument,
                      std::vector<float>& buffer,
                      Note& note,
                      int ticks,
                      float secondPerTick,
                      float detuneLeft,
                      float detuneRight,
                      bool makeDirty)
{
    for (int i = 0; i < buffer.size(); i += 2) {
        int left = i;
        int right = i + 1;
        float timeInSeconds = static_cast<float> (ticks + i/2) * secondPerTick;
        float level = note.envelope.level (elapsedSeconds());

        level *= note.velocity;
        buffer[left] = .0f;
        buffer[right] = .0f;

        switch (instrument) {
            case 0 : {
                buffer[left]  = oscSine (keyToPitch (note.noteId,
                                                           detuneLeft),
                                               timeInSeconds);
                buffer[right] = oscSine (keyToPitch (note.noteId,
                                                           detuneRight),
                                               timeInSeconds);

                buffer[left]  += oscSine (keyToPitch (note.noteId,
                                                           detuneLeft*1.5f),
                                               timeInSeconds);
                buffer[right] += oscSine (keyToPitch (note.noteId,
                                                           detuneRight*1.5f),
                                               timeInSeconds);

                buffer[left]  += oscSine (keyToPitch (note.noteId,
                                                           detuneLeft*3.f),
                                               timeInSeconds);
                buffer[right] += oscSine (keyToPitch (note.noteId,
                                                           detuneRight*3.f),
                                               timeInSeconds);

                buffer[left]  += oscSine (keyToPitch (note.noteId,
                                                           detuneLeft*4.5f),
                                               timeInSeconds);
                buffer[right] += oscSine (keyToPitch (note.noteId,
                                                           detuneRight*4.5f),
                                               timeInSeconds);
                break;
            }

            case 1 : {
                buffer[left]  = oscSquare (keyToPitch (note.noteId,
                                                       detuneLeft),
                                           timeInSeconds);
                buffer[right] = oscSquare (keyToPitch (note.noteId,
                                                       detuneRight),
                                           timeInSeconds);

                buffer[left]  += oscSquare (keyToPitch (note.noteId,
                                                        detuneLeft*1.5f),
                                            timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*1.5f),
                                            timeInSeconds);

                buffer[left]  += oscSquare (keyToPitch (note.noteId,
                                                        detuneLeft*3.f),
                                            timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*3.f),
                                            timeInSeconds);

                buffer[left]  += oscSquare (keyToPitch (note.noteId,
                                                        detuneLeft*4.5f),
                                            timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*4.5f),
                                            timeInSeconds);
                break;
            }

            case 2 : {
                buffer[left]  = oscSawtooth (keyToPitch (note.noteId,
                                                         detuneLeft),
                                             timeInSeconds);
                buffer[right] = oscSawtooth (keyToPitch (note.noteId,
                                                         detuneRight),
                                             timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*1.5f),
                                              timeInSeconds);
                buffer[right] += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneRight*1.5f),
                                              timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*3.f),
                                              timeInSeconds);
                buffer[right] += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneRight*3.f),
                                              timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*4.5f),
                                              timeInSeconds);
                buffer[right] += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneRight*4.5f),
                                              timeInSeconds);
                break;
            }

            case 3 : {
                buffer[left]  = oscSawtooth (keyToPitch (note.noteId,
                                                         detuneLeft),
                                             timeInSeconds);
                buffer[right] = oscSquare (keyToPitch (note.noteId,
                                                       detuneRight),
                                           timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*1.5f),
                                              timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*1.5f),
                                            timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*3.f),
                                              timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*3.f),
                                            timeInSeconds);

                buffer[left]  += oscSawtooth (keyToPitch (note.noteId,
                                                          detuneLeft*4.5f),
                                              timeInSeconds);
                buffer[right] += oscSquare (keyToPitch (note.noteId,
                                                        detuneRight*4.5f),
                                            timeInSeconds);
                break;
            }

            default :
            break;
        }

        buffer[left] *= level;
        buffer[right] *= level;

        if (makeDirty) {
            buffer[left] += .125*oscNoise();
            buffer[right] += .125*oscNoise();
        }
    }
}

static bool makeDirty = false;
static short instrument = 0;
static short numBuffersPerSecond = 0;

void fillSampleBuffer (void* userdata, Uint8* stream, int lengthInBytes)
{
    std::lock_guard<std::mutex> guard(synthDataMutex);

    auto start = std::chrono::steady_clock::now();

    SynthData* synthData = reinterpret_cast<SynthData*> (userdata);
    float secondPerTick = 1.f/static_cast<float> (synthData->sampleRate);
    float volume = synthData->volume;
    SDL_memset (stream, 0, lengthInBytes);
    int sizePerSample = static_cast<int> (sizeof (float));
    shared_ptr<vector<vector<float>>> voiceBuffers = synthData->voiceBuffers;
    float* sampleBuffer = reinterpret_cast<float*> (stream);

    int voice = 0;
    float detuneLeft = 20.f*(.5f + .5f*sin (w (.025f)));
    float detuneRight = 10.f*(.5f + .5f*sin (w (.025f)));
    std::vector<std::thread> threads;
    for (auto note : *synthData->notes) {
        threads.push_back (std::thread (fillVoiceBuffer,
                                        instrument,
                                        std::ref (voiceBuffers->at(voice)),
                                        std::ref (note),
                                        synthData->ticks,
                                        secondPerTick,
                                        detuneLeft,
                                        detuneRight,
                                        makeDirty));
        ++voice;
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
        int left = i;
        int right = i + 1;
        float sumLeft = .0f;
        float sumRight = .0f;

        for (int voice = 0; voice < synthData->notes->size(); ++voice) {
            sumLeft += voiceBuffers->at(voice)[left];
            sumRight += voiceBuffers->at(voice)[right];
        }

        sampleBuffer[left] = volume*sumLeft;
        sampleBuffer[right] = volume*sumRight;

        synthData->sampleBufferForDrawing[left] = sampleBuffer[left];
        synthData->sampleBufferForDrawing[right] = sampleBuffer[right];
    }

    // float* sampleBuffer = reinterpret_cast<float*> (stream);
	/*for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
		timeInSeconds = static_cast<float> (synthData->ticks + i/2) * secondPerTick;
        sampleBuffer[i] = .0f;
        sampleBuffer[i+1] = .0f;

        for (auto note : *synthData->notes) {
            float level = note.envelope.level (elapsedSeconds());
            level *= note.velocity;

        }

        // synthData->filter->setCutoffFreqHZ(10000.f + 10000.f*(.5f + .5f*sin(w(1.f)*elapsedSeconds())));
        // sampleBuffer[i] = synthData->filter->filterIn(sampleBuffer[i]);
        // sampleBuffer[i+1] = synthData->filter->filterIn(sampleBuffer[i+1]);

	}*/

    synthData->ticks += ((lengthInBytes/sizePerSample - 1) / 2) + 1;
    ++numBuffersPerSecond;

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "ms: " << duration.count() << '\n';
}

Application::Application (size_t width, size_t height)
    : _initialized {false}
    , _window {nullptr}
    , _running {false}
    , _synth {Synth(_maxVoices)}
    , _sampleBufferForDrawing(_sampleBufferSize*2)
{
    initialize ();

    _voiceThreads.reserve(_maxVoices);
    _voiceBuffers.reserve(_maxVoices);
    for (size_t voice = 0; voice < _maxVoices; ++voice) {
        _voiceBuffers.push_back(std::vector<float> (_sampleBufferSize*_channels));
        std::fill_n (_voiceBuffers[voice].begin(),
                     _sampleBufferSize*_channels,
                     .0f);
    }

    SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute (SDL_GL_MULTISAMPLESAMPLES, 8);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_PROFILE_MASK,
                         SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute (SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_ClearError ();
    _window = SDL_CreateWindow (WIN_TITLE,
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED,
                                width,
                                height,
                                SDL_WINDOW_OPENGL);
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

    _synthData.filter = std::make_shared<Filter> (_cutOffFrequency,
                                                  1.f/48000.f,
                                                  IIR::ORDER::OD4);
    _synthData.filter->dumpParams();
    _synthData.sampleRate = _sampleRate;
    _synthData.ticks = 0;
    _synthData.volume = .1f;
    _synthData.notes = _synth.notes();
    _synthData.sampleBufferForDrawing = _sampleBufferForDrawing.data();
    _synthData.voiceBuffers = make_shared<vector<vector<float>>>(_voiceBuffers);

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

    SDL_ClearError (); 
    _context = SDL_GL_CreateContext (_window);
    if (!_context) {
        cout << "CreateContext() failed: " << SDL_GetError () << std::endl; 
        SDL_DestroyWindow (_window);
        SDL_Quit (); 
        _initialized = false;
        return;
    }

    _gl.reset(new OpenGL(width, height));
    _gl->init(_sampleBufferSize);
}

Application::~Application ()
{
    SDL_GL_DeleteContext (_context);
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

    {
        std::lock_guard<std::mutex> guard(midiMessageQueueMutex);
        if (_midiMessageQueue.size() > 0) {
            auto message = _midiMessageQueue.front();

            MessageType type = std::get<0>(message);
            char noteId = std::get<1> (message);
            char velocity = std::get<2> (message);
            float timeStamp = std::get<3> (message);

            _midiMessageQueue.pop();

            if (type == MessageType::NoteOff) {
                _synth.removeNoteMidi (static_cast<NoteId>(noteId - 20),
                                       static_cast<float>(velocity)/128.f,
                                       timeStamp);
            }

            if (type == MessageType::NoteOn) {
                _synth.addNoteMidi (static_cast<NoteId>(noteId - 20),
                                    static_cast<float>(velocity)/128.f,
                                    timeStamp);
            }
        }
    }

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

                case SDLK_F1: instrument = 0; break;
                case SDLK_F2: instrument = 1; break;
                case SDLK_F3: instrument = 2; break;
                case SDLK_F4: instrument = 3; break;
                case SDLK_F5: makeDirty = !makeDirty; break;
                case SDLK_PLUS : if (_synthData.volume <= .95f) {
                                     _synthData.volume += .05f;
                                     cout << "volume " << _synthData.volume << '\n';
                                 }
                                 break;
                case SDLK_MINUS : if (_synthData.volume >= .05f) {
                                     _synthData.volume -= .05f;
                                     cout << "volume " << _synthData.volume << '\n';
                                 }
                                 break;
                case SDLK_SPACE: {
                    _mute = !_mute;
                    SDL_PauseAudioDevice (_audioDevice, _mute);
                    break;
                }
                case SDLK_F6:
                    _cutOffFrequency += 100.f;
                    _synthData.filter->setCutoffFreqHZ(_cutOffFrequency);
                    _synthData.filter->dumpParams();
                break;
                case SDLK_F7:
                    _cutOffFrequency -= 100.f;
                    _synthData.filter->setCutoffFreqHZ(_cutOffFrequency);
                    _synthData.filter->dumpParams();
                break;

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

    auto start = std::chrono::steady_clock::now();

    while (_running) {
        handle_events ();
        update ();
         _synth.clearNotes ();

        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (duration.count() > 1.f) {
            std::cout << "buffers/second: " << numBuffersPerSecond << '\n';
            start = std::chrono::steady_clock::now();
            numBuffersPerSecond = 0;
        }
    }
}

void Application::update ()
{
    if (!_initialized)
        return;

    _gl->draw (_sampleBufferForDrawing);
    SDL_GL_SwapWindow(_window);
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

void Synth::addNoteMidi(NoteId noteId, float velocity, float timeStamp)
{
    if (_notes->size() < _maxVoices) {
        auto result = find_if (_notes->begin(),
                               _notes->end(),
                               [noteId] (Note note) {
                                   return note.noteId == noteId;
                               });
        if (result != _notes->end()) {
            if ((*result).envelope.noteReleased) {
                (*result).envelope.noteOn (timeStamp);
                (*result).velocity = velocity;
                (*result).envelope.noteOff (.0f);
            }
        } else {
            Note note;
            note.noteId = noteId;
            note.envelope.noteOn (timeStamp);
            note.velocity = velocity;
            _notes->emplace_back (note);
        }
    }
}

void Synth::removeNoteMidi(NoteId noteId, float velocity, float timeStamp)
{
    auto result = find_if (_notes->begin(),
                           _notes->end(),
                           [noteId] (Note note) {
                               return note.noteId == noteId;
                           });

    if (result != _notes->end()) {
        (*result).envelope.noteOff (timeStamp);
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
