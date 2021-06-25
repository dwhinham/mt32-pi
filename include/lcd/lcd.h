//
// lcd.h
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

#ifndef _lcd_h
#define _lcd_h

#include <circle/types.h>

#include "images.h"

class CLCD
{
public:
	enum class TType
	{
		Character,
		Graphical,
	};

	CLCD(u8 nWidth, u8 nHeight)
		: m_bBacklightEnabled(true),
		  m_nWidth(nWidth),
		  m_nHeight(nHeight)
	{
	}

	virtual ~CLCD() = default;

	virtual bool Initialize() = 0;
	virtual TType GetType() const = 0;
	u8 Width() const { return m_nWidth; }
	u8 Height() const { return m_nHeight; }

	// Character functions (available when type is 'Character' or 'Graphical')
	virtual void Clear(bool bImmediate = true) {};
	virtual void Print(const char* pText, u8 nCursorX = 0, u8 nCursorY = 0, bool bClearLine = false, bool bImmediate = true) {};

	// Graphics functions (available when type is 'Graphical')
	virtual void SetPixel(u8 nX, u8 nY) {};
	virtual void ClearPixel(u8 nX, u8 nY) {};
	virtual void DrawFilledRect(u8 nX1, u8 nY1, u8 nX2, u8 nY2, bool bImmediate = false) {};
	virtual void DrawChar(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted = false, bool bDoubleWidth = false) {};
	virtual void DrawImage(TImage Image, bool bImmediate = false) {};
	virtual void Flip() {};

	bool GetBacklightState() const { return m_bBacklightEnabled; }
	virtual void SetBacklightState(bool bEnabled) {};

protected:
	bool m_bBacklightEnabled;
	u8 m_nWidth;
	u8 m_nHeight;
};

#endif
