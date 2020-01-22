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

#define WIN_TITLE "16-way polyphonic synthesizer by MacSlow"
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

    _sampleBufferForDrawing.reserve (_sampleBufferSize*_channels);

    std::fill(_sampleBufferForDrawing.begin(),
              _sampleBufferForDrawing.end(),
              .0f);

    _initialized = true;

	if (_midi.initialized()) {
        std::thread midiKeyReadingThread (readMidiKeys,
										  std::ref (_midi),
										  std::ref(_midiMessageQueue));
        midiKeyReadingThread.detach();
	}

    if (_midi.initialized()) {
        std::thread midiDiscoThread (disco, std::ref (_midi));
        midiDiscoThread.detach();
    }
}

void Application::disco (const Midi& midi)
{
    while (true) {
        midi.padColorCycle();
    }
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

float customSin (float value)
{
    float x = value * .5f/M_PI;
    x -= floorf(x);
    return 20.785f*x*(x*x  - 1.5f*x + .5f);
    // return sinf (value);
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
        result += amplitude*customSin (w(frequency)*timeInSeconds);
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
        float level = note.amplitudeADSR.level (elapsedSeconds());

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
            case 4 : {
                buffer[left] = oscNoise();
                buffer[right] = oscNoise();
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

// assumes power-of-two number of samples, no zero-padding, no checks
void computeFFT (vector<complex<float>>::iterator begin,
                 vector<complex<float>>::iterator end)
{
    auto numSamples = distance (begin, end);

    if (numSamples < 2) {
        return;
    } else {
        // 'sort' even-indexed samples into the front half and odd ones in the
        // back half
        stable_partition (begin, end, [&begin] (auto& a) {
            return distance (&*begin, &a) % 2 == 0;
        });

        // recursion step
        computeFFT (begin, begin + numSamples/2); // even
        computeFFT (begin + numSamples/2, end); // odd

        // do the actual computation time->frequency domain
        for (decltype (numSamples) k = 0; k < numSamples/2; ++k) {
            auto even = *(begin + k);
            auto odd  = *(begin + k + numSamples/2);
            auto w = exp (complex<float>(.0f, -2.f*M_PI*k/numSamples))*odd;

            *(begin + k) = even + w;
            *(begin + k + numSamples/2) = even - w;
        }
    }
}

void computeFastFourierTransform (vector<float>& sampleBufferForDrawing,
                                  vector<float>& fftBufferForDrawing,
                                  float fromFrequency,
                                  float toFrequency,
                                  size_t frequencyBins,
                                  size_t samples,
                                  size_t sampleRate,
                                  size_t channels)
{
    size_t sampleBufferSize = sampleBufferForDrawing.size();
    float reciprocal = 5.f/static_cast<float> (samples);

    vector<complex<float>> leftChannel (sampleBufferSize/2, .0f);
    for (size_t i = 0; i < sampleBufferSize; i += 2) {
        leftChannel[i/2] = sampleBufferForDrawing[i];
    }

    computeFFT (leftChannel.begin(), leftChannel.end());

    for (size_t bin = 0; bin < frequencyBins; ++bin) {
        size_t left = 2*bin;
        fftBufferForDrawing[left] = reciprocal*sqrt (leftChannel[bin].real() *
                                                     leftChannel[bin].real() +
                                                     leftChannel[bin].imag() *
                                                     leftChannel[bin].imag());
    }
}

void fillSampleBuffer (void* userdata, Uint8* stream, int lengthInBytes)
{
    std::lock_guard<std::mutex> guard(synthDataMutex);

    // auto start = std::chrono::steady_clock::now();

    SynthData* synthData = reinterpret_cast<SynthData*> (userdata);
    float secondPerTick = 1.f/static_cast<float> (synthData->sampleRate);
    float volume = synthData->volume;
    SDL_memset (stream, 0, lengthInBytes);
    int sizePerSample = static_cast<int> (sizeof (float));
    shared_ptr<vector<vector<float>>> voiceBuffers = synthData->voiceBuffers;
    shared_ptr<vector<float>> sampleBufferForDrawing = synthData->sampleBufferForDrawing;
    shared_ptr<vector<float>> fftBufferForDrawing = synthData->fftBufferForDrawing;
    float* sampleBuffer = reinterpret_cast<float*> (stream);

    float detuneLeft = 20.f*(.5f + .5f*sin (w (.025f)));
    float detuneRight = 10.f*(.5f + .5f*sin (w (.025f)));
    std::vector<std::thread> threads;
    for (auto& note : *synthData->notes) {
        threads.push_back (std::thread (fillVoiceBuffer,
                                        instrument,
                                        std::ref (voiceBuffers->at(note.voice)),
                                        std::ref (note),
                                        synthData->ticks,
                                        secondPerTick,
                                        detuneLeft,
                                        detuneRight,
                                        makeDirty));
    }

    for (auto& t : threads) {
        t.join();
    }

    for (int i = 0; i < lengthInBytes/sizePerSample; i += 2) {
        int left = i;
        int right = i + 1;
        float sumLeft = .0f;
        float sumRight = .0f;

        for (const auto& note : *synthData->notes) {
            sumLeft += voiceBuffers->at(note.voice)[left];
            sumRight += voiceBuffers->at(note.voice)[right];
        }

        sampleBuffer[left] = volume*sumLeft;
        sampleBuffer[right] = volume*sumRight;

        (*sampleBufferForDrawing)[left] = sampleBuffer[left];
        (*sampleBufferForDrawing)[right] = sampleBuffer[right];
    }

    float fromFrequency = .0f;
    float toFrequency = 22'500.f;
    if (synthData->doFFT) {
        computeFastFourierTransform ((*sampleBufferForDrawing),
                                     (*fftBufferForDrawing),
                                     fromFrequency,
                                     toFrequency,
                                     synthData->frequencyBins,
                                     synthData->samples,
                                     synthData->sampleRate,
                                     synthData->channels);
    }

    synthData->ticks += ((lengthInBytes/sizePerSample - 1) / 2) + 1;
    ++numBuffersPerSecond;

    // auto end = std::chrono::steady_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    // std::cout << "ms: " << duration.count() << '\n';
}

Application::Application (size_t width, size_t height, const string& midiPort)
    : _initialized {false}
    , _window {nullptr}
    , _running {false}
    , _synth {Synth(_maxVoices)}
    , _sampleBufferForDrawing (_sampleBufferSize*_channels)
    , _fftBufferForDrawing (_frequencyBins*_channels)
	, _midi {midiPort}
{
    initialize ();

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

    _synthData.sampleRate = _sampleRate;
    _synthData.channels = _channels;
    _synthData.samples = _sampleBufferSize;
    _synthData.frequencyBins = _frequencyBins;
    _synthData.doFFT = false;
    _synthData.ticks = 0;
    _synthData.volume = .1f;
    _synthData.notes = _synth.notes();
    _synthData.sampleBufferForDrawing = make_shared<vector<float>>(_sampleBufferForDrawing);
    _synthData.fftBufferForDrawing = make_shared<vector<float>>(_fftBufferForDrawing);
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
    _gl->init(_sampleBufferSize*_channels, _frequencyBins*_channels);
}

Application::~Application ()
{
	if (_midi.initialized()) {
		for (unsigned char pad = 0; pad < 16; ++pad) {
			_midi.setPadColor (pad, PadColor::Black);
		}
	}

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

	if (_midi.initialized()) {
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
                case SDLK_F5: instrument = 4; break;
                case SDLK_F6: makeDirty = !makeDirty; break;
                case SDLK_F7: _synthData.doFFT = !_synthData.doFFT; break;
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

    _gl->draw (*_synthData.sampleBufferForDrawing,
               *_synthData.fftBufferForDrawing,
               _synthData.doFFT);
    SDL_GL_SwapWindow(_window);
}

Synth::Synth (unsigned int maxVoices)
    : _notes {std::make_shared<Notes>()}
    , _maxVoices {maxVoices}
{
    for (int voice = 0; voice < _maxVoices; ++voice) {
        _voiceAllocation.push_back(false);
    }
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
            if ((*result).amplitudeADSR.noteReleased) {
                (*result).amplitudeADSR.noteOn (elapsedSeconds());
                (*result).amplitudeADSR.noteOff (.0f);
                (*result).filterADSR.noteOn (elapsedSeconds());
                (*result).filterADSR.noteOff (.0f);
            }
        } else {
            Note note;
            note.noteId = noteId;
            note.amplitudeADSR.noteOn (elapsedSeconds());
            note.filterADSR.noteOn (elapsedSeconds());
            note.filterADSR.attackTime = .5f;
            note.filterADSR.decayTime = .1f;
            note.filterADSR.sustainLevel = .7f;
            note.filterADSR.releaseTime = 1.f;
            note.voice = allocVoice();
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
        (*result).amplitudeADSR.noteOff (elapsedSeconds());
        (*result).filterADSR.noteOff (elapsedSeconds());
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
            if ((*result).amplitudeADSR.noteReleased) {
                (*result).amplitudeADSR.noteOn (timeStamp);
                (*result).filterADSR.noteOn (timeStamp);
                (*result).velocity = velocity;
                (*result).amplitudeADSR.noteOff (.0f);
                (*result).filterADSR.noteOff (.0f);
            }
        } else {
            Note note;
            note.noteId = noteId;
            note.amplitudeADSR.noteOn (timeStamp);
            note.filterADSR.noteOn (timeStamp);
            note.filterADSR.attackTime = .5f;
            note.filterADSR.decayTime = .1f;
            note.filterADSR.sustainLevel = .7f;
            note.filterADSR.releaseTime = 1.f;
            note.velocity = velocity;
            note.voice = allocVoice();
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
        (*result).amplitudeADSR.noteOff (timeStamp);
        (*result).filterADSR.noteOff (timeStamp);
    }
}

void Synth::clearNotes ()
{
    std::lock_guard<std::mutex> guard(synthDataMutex);
    _notes->remove_if([this](Note& note){
        bool flag = note.amplitudeADSR.level(elapsedSeconds()) < .0001f;
        if (flag) {
            freeVoice(note.voice);
        }
        return flag;
    });
}

std::shared_ptr<Notes> Synth::notes()
{
    return _notes;
}

int Synth::allocVoice()
{
    auto result = find (_voiceAllocation.begin(), _voiceAllocation.end(), false);
    if (result != _voiceAllocation.end()) {
        int index = std::distance(_voiceAllocation.begin(), result);
        _voiceAllocation[index] = true;
        return index;
    } else {
        return -1;
    }
}

void Synth::freeVoice(int voice)
{
    _voiceAllocation[voice] = false;
}
