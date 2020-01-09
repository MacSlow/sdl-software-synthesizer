#ifndef _MIDI_H
#define _MIDI_H

#include <string>
#include <tuple>

#include <alsa/asoundlib.h>

enum MessageType { NoteOff = 0x80,
				   NoteOn = 0x90,
				   AfterTouch = 0xA0,
				   Controller = 0xB0,
				   PatchChange = 0xC0,
				   ChannelPressure = 0xD0,
				   PitchBend = 0xE0,
				   MiscCommands = 0xF0,
				   None = 0x00 };

using  MessageData = std::tuple<MessageType, char, char, float>;

class Midi
{
	public:
		explicit Midi (const std::string& port = "hw:1,0,0");
		~Midi ();

		MessageData read () const;
		bool initialized () const;

	private:
		snd_rawmidi_t* _midiPort;
		bool _initialized;
};

#endif // _MIDI_H
