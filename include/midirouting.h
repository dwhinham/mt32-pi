//
// midirouting.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
//
// This file is part of mt32-pi.
//
// mt32-pi is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// mt32-pi is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// mt32-pi. If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _midirouting_h
#define _midirouting_h

#include <circle/types.h>

enum TMIDIRoutingDest : u8
{
	None	  = 0,
	Synth     = (1 << 0),
	GPIO      = (1 << 1),
	Pisound   = (1 << 2),
	USBMIDI   = (1 << 3),
	USBSerial = (1 << 4),
	RTP       = (1 << 5),
	UDP       = (1 << 6),
};

using TMIDIRouting = u8;

#endif
