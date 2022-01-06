//
// sysex.h
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

#ifndef _sysex_h
#define _sysex_h

#include <circle/types.h>

enum TManufacturerID : u8
{
	Roland               = 0x41,
	Yamaha               = 0x43,
	UniversalNonRealTime = 0x7E,
	UniversalRealTime    = 0x7F,
};

enum TDeviceID : u8
{
	SoundCanvasDefault = 0x10, // The default device ID of Roland Sound Canvas modules
	AllCall            = 0x7F,
};

enum TUniversalSubID : u8
{
	GeneralMIDI = 0x09,
};

enum TGeneralMIDISubID : u8
{
	GeneralMIDIOn  = 0x01,
	GeneralMIDIOff = 0x02,
};

#endif
