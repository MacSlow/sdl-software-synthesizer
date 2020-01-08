#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <SDL.h>

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

#include "opengl.h"
#include "midi.h"
#include "filters.h"

using std::vector;
using std::map;
using std::queue;
using std::shared_ptr;
using std::list;
using std::thread;

using NoteId = int;

struct Envelope
{
    float attackLevel = 1.f;
    float attackTime = .15f;
    float decayTime = .2f;
    float sustainLevel = .8f;
    float releaseTime = .65f;
    float noteOnTime = .0f;
    float noteOffTime = .0f;
    bool noteActive = false;
    bool noteReleased = false;

    void noteOn (float currentTime)
    {
        noteOnTime = currentTime;
    }

    void noteOff (float currentTime)
    {
        noteOffTime = currentTime;
    }

    float level (float currentTime)
    {
        float outputLevel = .0;

        // handle attack, decay & sustain
        if (noteOnTime > noteOffTime) {
            float noteLifetime = currentTime - noteOnTime;

            // attack
            if (noteLifetime <= attackTime) {
                outputLevel = (noteLifetime / attackTime) * attackLevel;
            }

            // decay
            if (noteLifetime > attackTime &&
                noteLifetime <= attackTime + decayTime) {
                float attackSustainLevelDiff = attackLevel - sustainLevel;
                outputLevel = (1.f - (noteLifetime - attackTime) / decayTime) * attackSustainLevelDiff + sustainLevel;
            }

            // sustain
            if (noteLifetime > attackTime + decayTime) {
                outputLevel = sustainLevel;
            }
        } else if (noteOnTime <= noteOffTime) {
            // handle release
            noteReleased = true;
            float releaseLifetime = currentTime - noteOffTime;
            outputLevel = (1.f - (releaseLifetime / releaseTime)) * sustainLevel;
        }

        // switch off check
        if (outputLevel < .0001f) {
            outputLevel = .0f;
            noteActive = false;
        } else {
            noteActive = true;
        }

        return outputLevel;
    }
};

struct Note
{
    NoteId noteId;
    Envelope envelope;
    float velocity = 1.f;
};

using Notes = list<Note>;

class Synth
{
    public:
        explicit Synth(unsigned int maxVoices = 16);

        void addNote(NoteId note);
        void removeNote(NoteId note);

        void addNoteMidi(NoteId noteId, float velocity, float timeStamp);
        void removeNoteMidi(NoteId noteId, float velocity, float timeStamp);

        shared_ptr<Notes> notes();
        void clearNotes ();

    private:
        shared_ptr<Notes> _notes;
        unsigned int _maxVoices;
};

struct SynthData
{
    float sampleRate;
    int ticks;
    float volume;
    shared_ptr<Notes> notes;
    float* sampleBufferForDrawing;
    shared_ptr<Filter> filter;
    shared_ptr<vector<vector<float>>> voiceBuffers;
};

struct MidiMessage
{
    MessageType type;
    NoteId noteId;
    float velocity;
    float timeStamp;
};

class Application
{
    public:
        Application (size_t width, size_t height);
        ~Application ();

        void run ();
        void update ();

    private:
        void initialize ();
        void handle_events ();
        static void readMidiKeys (const Midi& midi,
                                  queue<MessageData>& queue);

    private:
        bool _initialized = false;
        SDL_Window* _window = nullptr;
        SDL_GLContext _context;
        bool _running = true;
        SDL_AudioDeviceID _audioDevice;
        bool _mute = false;
        int _sampleRate = 48000;
        int _channels = 2;
        int _sampleBufferSize = 512;
        unsigned int _maxVoices = 12;
        float _cutOffFrequency = 22000.f;
        Synth _synth;
        shared_ptr<Notes> _notes;
        SynthData _synthData;
        map<SDL_Keycode, bool> _pressedKeys;
        vector<float> _sampleBufferForDrawing;
        shared_ptr<OpenGL> _gl;
        Midi _midi;
        queue<MessageData> _midiMessageQueue;
        vector<thread> _voiceThreads;
        vector<vector<float>> _voiceBuffers;
};

#endif // _APPLICATION_H
