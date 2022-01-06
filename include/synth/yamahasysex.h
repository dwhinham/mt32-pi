//
// yamahasysex.h
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

#ifndef _yamahasysex_h
#define _yamahasysex_h

#include "synth/sysex.h"

enum TYamahaModelID : u8
{
	XG = 0x4C,
};

enum TYamahaAddress : u32
{
	XGSystemOn = 0x00007E,

	DisplayLetter = 0x060000,
	DisplayBitmap = 0x070000,
};

enum TYamahaAddressMask : u32
{
	DisplayLetterMask = 0xFFFF00,
};

struct TYamahaSysExHeader
{
	TManufacturerID ManufacturerID;
	TDeviceID DeviceID;
	TYamahaModelID ModelID;
	u8 Address[3];
};

#endif
