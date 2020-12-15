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

#include "lcd/synthlcd.h"
#include "synth/mt32synth.h"
#include "utility.h"

class CSSD1306 : public CSynthLCD
{
public:
	#define ENUM_LCDROTATION(ENUM) \
		ENUM(Normal, normal)       \
		ENUM(Inverted, inverted)

	CONFIG_ENUM(TLCDRotation, ENUM_LCDROTATION);

	CSSD1306(CI2CMaster* pI2CMaster, u8 nAddress = 0x3c, u8 nWidth = 128, u8 nHeight = 32, TLCDRotation Rotation = TLCDRotation::Normal);

	// CCharacterLCD
	virtual bool Initialize() override;
	virtual void Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine = false, bool bImmediate = false) override;
	virtual void Clear(bool bImmediate = false) override;
	virtual void SetBacklightEnabled(bool bEnabled) override;

	// CSynthLCD
	virtual void Update(CMT32Synth& Synth) override;
	virtual void Update(CSoundFontSynth& Synth) override;

private:
	struct TFrameBufferUpdatePacket
	{
		u8 DataControlByte;
		u8 FrameBuffer[128 * 64 / 8];
	}
	PACKED;

	void WriteCommand(u8 nCommand) const;
	void WriteFrameBuffer() const;
	void SwapFrameBuffers();
	void SetPixel(u8 nX, u8 nY);
	void ClearPixel(u8 nX, u8 nY);
	void DrawChar(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted = false, bool bDoubleWidth = false);

	void DrawChannelLevels(u8 nFirstRow, u8 nRows, u8 nBarXOffset, u8 nBarWidth, u8 nBarSpacing, u8 nChannels, bool bDrawPeaks = true, bool bDrawBarBases = true);

	CI2CMaster* m_pI2CMaster;
	u8 m_nAddress;
	u8 m_nWidth;
	u8 m_nHeight;
	TLCDRotation m_Rotation;

	// Double framebuffers
	TFrameBufferUpdatePacket m_FrameBuffers[2];
	u8 m_nCurrentFrameBuffer;
};

#endif
