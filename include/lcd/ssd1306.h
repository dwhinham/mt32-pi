//
// ssd1306.h
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

#ifndef _ssd1306_h
#define _ssd1306_h

#include <circle/i2cmaster.h>
#include <circle/types.h>

#include "lcd/mt32lcd.h"
#include "mt32synth.h"

class CSSD1306 : public CMT32LCD
{
public:
	CSSD1306(CI2CMaster* pI2CMaster, u8 pAddress = 0x3c, u8 pHeight = 32);

	// CCharacterLCD
	virtual bool Initialize() override;
	virtual void Print(const char* pText, u8 pCursorX, u8 pCursorY, bool pClearLine = false, bool pImmediate = false) override;
	virtual void Clear() override;

	// CMT32LCD
	virtual void Update(const CMT32SynthBase& pSynth) override;

private:
	void WriteFramebuffer() const;
	void SetPixel(u8 pX, u8 pY);
	void ClearPixel(u8 pX, u8 pY);
	void DrawChar(char pChar, u8 pCursorX, u8 pCursorY, bool pInverted = false, bool pDoubleWidth = false);

	void DrawPartLevels(bool pDrawPeaks = true);

	CI2CMaster* mI2CMaster;
	u8 mAddress;
	u8 mHeight;

	// +1 to store the 0x40 command at the beginning
	u8 mFramebuffer[128 * 64 / 8 + 1];
	
	static const u8 InitSequence[];
};

#endif
