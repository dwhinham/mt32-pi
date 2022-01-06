//
// misterstatus.h
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

#ifndef _misterstatus_h
#define _misterstatus_h

#include <circle/types.h>

#include "synth/synth.h"

enum class TMisterSynth : u8
{
	Mute      = 0xA0,
	MT32      = 0xA1,
	SoundFont = 0xA2,
	Unknown   = 0xFF,
};

struct TMisterStatus
{
	TMisterSynth Synth;
	u8 MT32ROMSet;
	u8 SoundFontIndex;
}
PACKED;

inline bool operator==(const TMisterStatus& LHS, const TMisterStatus& RHS)
{
	return LHS.Synth == RHS.Synth && LHS.MT32ROMSet == RHS.MT32ROMSet && LHS.SoundFontIndex == RHS.SoundFontIndex;
}

inline bool operator!=(const TMisterStatus& LHS, const TMisterStatus& RHS)
{
	return !operator==(LHS, RHS);
}

#endif
