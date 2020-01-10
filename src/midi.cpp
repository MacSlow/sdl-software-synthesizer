#include <cmath>
#include <iostream>
#include <vector>

#include <SDL.h>

#include "midi.h"

Midi::Midi (const std::string& port)
	: _midiPortInput {nullptr}
	, _midiPortOutput {nullptr}
	, _initialized {false}
{
	int result = snd_rawmidi_open (&_midiPortInput,
								   &_midiPortOutput,
								   port.c_str(),
								   SND_RAWMIDI_SYNC);
	if (result == 0) {
		_initialized = true;
	}
}

void Midi::setPadColor (const unsigned char padNum, const PadColor color) const
{
	// Found this SysEx command on on the Arturia user-forums.
	// F0 00 20 6B 7F 42 02 00 10 7n cc F7
	// n is the pad number, 0 to F, corresponding to Pad1 to Pad16
	//
	// cc is the color:
    //     00 - black
    //     01 - red
    //     04 - green
    //     05 - yellow
    //     10 - blue
    //     11 - magenta
    //     14 - cyan
    //     7F - white
    unsigned char pad = 0x70 + padNum;
    unsigned char col = static_cast<unsigned char> (color);
    std::vector<unsigned char> buffer = {0xF0, 0x00, 0x20, 0x6B,
										 0x7F, 0x42, 0x02, 0x00,
										 0x10, pad, col, 0xF7};
	ssize_t result = snd_rawmidi_write (_midiPortOutput,
										static_cast<void*> (buffer.data()),
										buffer.size());
	if (result != buffer.size()) {
		std::cout << "Error: Wrote " << result
				  << " bytes, expected " << buffer.size() << '\n';
	}
}

MessageData Midi::read () const
{
 	char buffer[1];
 	char buffer2[2];
 	float timeStamp = .0f;
 	MessageData messageData = std::make_tuple(MessageType::None, 0, 0, .0f);
	int result = snd_rawmidi_read (_midiPortInput, buffer, 1);
	if (result > 0) {
		unsigned char midiMessage = static_cast<unsigned char>(buffer[0]);
		switch (midiMessage) {
			case MessageType::NoteOff:
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
				timeStamp = static_cast<float>(SDL_GetTicks())*.001f;
				messageData = std::make_tuple (MessageType::NoteOff,
											  buffer2[0],
											  buffer2[1],
											  timeStamp);
			break;

			case MessageType::NoteOn:
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
				timeStamp = static_cast<float>(SDL_GetTicks())*.001f;
				messageData = std::make_tuple (MessageType::NoteOn,
											   buffer2[0],
											   buffer2[1],
											   timeStamp);
			break;

			case MessageType::AfterTouch:
				// just consume the input
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
			break;

			case MessageType::Controller:
				// just consume the input
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
			break;

			case PatchChange:
				// just consume the input
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
			break;

			case ChannelPressure:
				// just consume the input
				result = snd_rawmidi_read (_midiPortInput, buffer, 1);
			break;

			case PitchBend:
				// just consume the input
				result = snd_rawmidi_read (_midiPortInput, buffer2, 2);
			break;
		}
	}

	return messageData;
}

Midi::~Midi ()
{
	if (_initialized) {
		snd_rawmidi_close (_midiPortInput);
		snd_rawmidi_close (_midiPortOutput);
		_midiPortInput = nullptr;
		_midiPortOutput = nullptr;
	}
}

bool Midi::initialized () const
{
	return _initialized;
}

// A little disco-effect for the drum-pads on the Arturia MiniLab mkII
void Midi::padColorCycle () const
{
	int pause = 5;
	unsigned char start = 0;
	unsigned char end = 8;
	std::vector<PadColor> colors = {PadColor::White,
									PadColor::Yellow,
									PadColor::Red,
									PadColor::Magenta,
									PadColor::Blue,
									PadColor::Cyan,
									PadColor::Green};

	int index = 0;
	for (auto color : colors) {
		for (unsigned char foo = start; foo < end; ++foo) {
			std::ignore = color;
			++index;
			for (unsigned char pad = start; pad < end; ++pad) {
				setPadColor (pad, pad == foo ? colors[index%7] : PadColor::Black);
				SDL_Delay (pause);
			}
		}

		for (unsigned char foo = start + 2; foo <= end; ++foo) {
			for (unsigned char pad = start; pad < end; ++pad) {
				setPadColor (pad, pad == (end - foo) ? colors[index%7] : PadColor::Black);
				SDL_Delay (pause);
			}
		}

		setPadColor (start, PadColor::Black);
		SDL_Delay (pause);
	}
}