//
// clcd.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2021 Dale Whinham <daleyo@gmail.com>
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

#ifndef _clcd_h
#define _clcd_h

#include <circle/types.h>

class CCharacterLCD
{
public:
	CCharacterLCD()
		: m_bBacklightEnabled(true)
	{
	}

	virtual ~CCharacterLCD() = default;

	virtual bool Initialize() = 0;
	virtual void Print(const char* pText, u8 nCursorX = 0, u8 nCursorY = 0, bool bClearLine = false, bool bImmediate = true) = 0;
	virtual void Clear(bool bImmediate = true) = 0;
	virtual void SetBacklightEnabled(bool bEnabled) { m_bBacklightEnabled = bEnabled; }

protected:
	bool m_bBacklightEnabled;
};

#endif
