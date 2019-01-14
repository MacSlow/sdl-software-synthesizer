#ifndef _APPLICATION_H
#define _APPLICATION_H

#include <list>
#include <memory>
#include <vector>

#include <SDL.h>

#include "software-synthesizer.h"

struct Envelope
{
    float attackLevel = 1.f;
    float attackTime = .1f;
    float decayTime = .1f;
    float sustainLevel = .5f;
    float releaseTime = .1f;
    float noteOnTime = .0f;
    float noteOffTime = .0f;
    bool noteActive = false;

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
    float frequency;
    Envelope envelope;
};

struct SynthData
{
	float sampleRate;
	int ticks;
	float volume;
    std::shared_ptr<std::list<int>> notes;
};

class Application {
    public:
        Application (size_t width, size_t height);
        ~Application ();

        void run ();
        void update ();
        void output ();

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
        int _maxVoices = 7;
        std::shared_ptr<std::list<int>> _notes;
        SynthData _synthData;
};

#endif // _APPLICATION_H
