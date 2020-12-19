//
// event.h
//
// mt32-pi - A bare-metal Roland MT-32 emulator for Raspberry Pi
// Copyright (C) 2020  Dale Whinham <daleyo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _event_h
#define _event_h

#include "control/button.h"
#include "ringbuffer.h"
#include "synth/mt32romset.h"
#include "synth/synth.h"

struct TButtonEvent
{
	TButton Button;
	bool bPressed;
};

struct TEncoderEvent
{
	s8 nDelta;
};

struct TSwitchSynthEvent
{
	TSynth Synth;
};

struct TSwitchMT32ROMSetEvent
{
	TMT32ROMSet ROMSet;
};

struct TSwitchSoundFontEvent
{
	size_t Index;
};

struct TAllSoundOffEvent
{
	// No payload
};

enum class TEventType
{
	Button,
	Encoder,
	SwitchSynth,
	SwitchMT32ROMSet,
	SwitchSoundFont,
	AllSoundOff
};

struct TEvent
{
	TEventType Type;

	union
	{
		TButtonEvent Button;
		TEncoderEvent Encoder;
		TSwitchSynthEvent SwitchSynth;
		TSwitchMT32ROMSetEvent SwitchMT32ROMSet;
		TSwitchSoundFontEvent SwitchSoundFont;
		TAllSoundOffEvent AllSoundOff;
	};
};

constexpr size_t EventQueueSize = 32;
using TEventQueue               = CRingBuffer<TEvent, EventQueueSize>;

#endif
