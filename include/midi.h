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

enum PadColor { Black = 0x00,
				Red = 0x01,
				Green = 0x04,
				Yellow = 0x05,
				Blue = 0x10,
				Magenta = 0x11,
				Cyan = 0x14,
				White = 0x7F };

using  MessageData = std::tuple<MessageType, char, char, float>;

class Midi
{
	public:
		explicit Midi (const std::string& port = "hw:1,0,0");
		~Midi ();

		bool initialized () const;
		MessageData read () const;
		void setPadColor (const unsigned char padNum,
						  const PadColor color) const;
		void padColorCycle () const;

	private:
		snd_rawmidi_t* _midiPortInput;
		snd_rawmidi_t* _midiPortOutput;
		bool _initialized;
};

#endif // _MIDI_H
