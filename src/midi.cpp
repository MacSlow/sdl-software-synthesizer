#include <cmath>
#include <iostream>

#include <SDL.h>

#include "midi.h"

Midi::Midi (const std::string& port)
	: _midiPort {nullptr}
	, _initialized {false}
{
	int result = snd_rawmidi_open (&_midiPort,
								   nullptr,
								   port.c_str(),
								   SND_RAWMIDI_SYNC);
	if (result == 0) {
		_initialized = true;
	}
}

MessageData Midi::read () const
{
 	char buffer[1];
 	char buffer2[2];
 	float timeStamp = .0f;
 	MessageData messageData = std::make_tuple(MessageType::None, 0, 0, .0f);
	int result = snd_rawmidi_read (_midiPort, buffer, 1);
	if (result > 0) {
		unsigned char midiMessage = static_cast<unsigned char>(buffer[0]);
		switch (midiMessage) {
			case MessageType::NoteOff:
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
				timeStamp = static_cast<float>(SDL_GetTicks())*.001f;
				messageData = std::make_tuple (MessageType::NoteOff,
											  buffer2[0],
											  buffer2[1],
											  timeStamp);
			break;

			case MessageType::NoteOn:
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
				timeStamp = static_cast<float>(SDL_GetTicks())*.001f;
				messageData = std::make_tuple (MessageType::NoteOn,
											   buffer2[0],
											   buffer2[1],
											   timeStamp);
			break;

			case MessageType::AfterTouch:
				// just consume the input
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
			break;

			case MessageType::Controller:
				// just consume the input
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
			break;

			case PatchChange:
				// just consume the input
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
			break;

			case ChannelPressure:
				// just consume the input
				result = snd_rawmidi_read (_midiPort, buffer, 1);
			break;

			case PitchBend:
				// just consume the input
				result = snd_rawmidi_read (_midiPort, buffer2, 2);
			break;
		}
	}

	return messageData;
}

Midi::~Midi ()
{
	if (_initialized) {
		snd_rawmidi_close (_midiPort);
		_midiPort = nullptr;
	}
}

bool Midi::initialized () const
{
	return _initialized;
}
