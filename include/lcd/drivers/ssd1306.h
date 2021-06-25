//
// ssd1306.h
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

#ifndef _ssd1306_h
#define _ssd1306_h

#include <circle/i2cmaster.h>
#include <circle/types.h>

#include "lcd/lcd.h"
#include "synth/mt32synth.h"
#include "utility.h"

class CSSD1306 : public CLCD
{
public:
	#define ENUM_LCDROTATION(ENUM) \
		ENUM(Normal, normal)       \
		ENUM(Inverted, inverted)

	CONFIG_ENUM(TLCDRotation, ENUM_LCDROTATION);

	CSSD1306(CI2CMaster* pI2CMaster, u8 nAddress = 0x3C, u8 nWidth = 128, u8 nHeight = 32, TLCDRotation Rotation = TLCDRotation::Normal);

	// CLCD
	virtual bool Initialize() override;
	virtual TType GetType() const override { return TType::Graphical; };

	// Character functions
	virtual void Clear(bool bImmediate = false) override;
	virtual void Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine = false, bool bImmediate = false) override;

	// Graphics functions
	virtual void SetPixel(u8 nX, u8 nY) override;
	virtual void ClearPixel(u8 nX, u8 nY) override;
	virtual void DrawFilledRect(u8 nX1, u8 nY1, u8 nX2, u8 nY2, bool bImmediate = false) override;
	virtual void DrawChar(char chChar, u8 nCursorX, u8 nCursorY, bool bInverted = false, bool bDoubleWidth = false) override;
	virtual void DrawImage(TImage Image, bool bImmediate = false) override;
	virtual void Flip() override;

	virtual void SetBacklightState(bool bEnabled) override;

protected:
	struct TFrameBufferUpdatePacket
	{
		u8 DataControlByte;
		u8 FrameBuffer[128 * 64 / 8];
	}
	PACKED;

	void WriteCommand(u8 nCommand) const;
	virtual void WriteFrameBuffer(bool bForceFullUpdate = false) const;
	void SwapFrameBuffers();

	CI2CMaster* m_pI2CMaster;
	u8 m_nAddress;
	TLCDRotation m_Rotation;

	// Double framebuffers
	TFrameBufferUpdatePacket m_FrameBuffers[2];
	u8 m_nCurrentFrameBuffer;
};

class CSH1106 : public CSSD1306
{
public:
	CSH1106(CI2CMaster* pI2CMaster, u8 nAddress = 0x3C, u8 nWidth = 128, u8 nHeight = 32, TLCDRotation Rotation = TLCDRotation::Normal);

private:
	void WriteData(const u8* pData, size_t nSize) const;
	virtual void WriteFrameBuffer(bool bForceFullUpdate = false) const override;
};

#endif
