//
// gmsysex.h
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

#ifndef _gmsysex_h
#define _gmsysex_h

#include "synth/sysex.h"

enum class TGMSubID : u8
{
	GeneralMIDIOn  = 0x01,
	GeneralMIDIOff = 0x02,
};

struct TGMSysExHeader
{
	TManufacturerID ManufacturerID;
	TDeviceID DeviceID;
	TUniversalSubID SubID1;
	TGMSubID SubID2;
}
PACKED;

#endif
