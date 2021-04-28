//
// hd44780.h
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

#ifndef _hd44780_h
#define _hd44780_h

#include <circle/gpiopin.h>
#include <circle/i2cmaster.h>
#include <circle/sched/scheduler.h>
#include <circle/types.h>

#include "synth/mt32synth.h"

class CHD44780Base : public CLCD
{
public:
	CHD44780Base(u8 nColumns = 20, u8 nRows = 2);
	virtual ~CHD44780Base() = default;

	virtual bool Initialize() override;
	virtual TType GetType() const override { return TType::Character; }

	// Character functions
	virtual void Clear(bool bImmediate = true) override;
	virtual void Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine = false, bool bImmediate = true) override;

protected:
	enum class TWriteMode
	{
		Data,
		Command
	};

	enum class TBarCharSet
	{
		None,
		Wide,
		Narrow
	};

	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) = 0;
	void WriteByte(u8 nByte, TWriteMode Mode);

	void WriteCommand(u8 nByte);
	void WriteData(u8 nByte);
	void WriteData(const u8* pBytes, size_t nSize);

	void SetCustomChar(u8 nIndex, const u8 nCharData[8]);
	void SetBarChars(TBarCharSet CharSet);
	void DrawChannelLevels(u8 nFirstRow, u8 nRows, u8 nBarOffsetX, u8 nBarSpacing, u8 nChannels, bool bDrawBarBases = true);

	u8 m_RowOffsets[4];

	TBarCharSet m_BarCharSet;
};

class CHD44780FourBit : public CHD44780Base
{
public:
	CHD44780FourBit(u8 nColumns = 20, u8 nRows = 2);

protected:
	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) override;

	CGPIOPin m_RS;
	CGPIOPin m_RW;
	CGPIOPin m_EN;
	CGPIOPin m_D4;
	CGPIOPin m_D5;
	CGPIOPin m_D6;
	CGPIOPin m_D7;
};

class CHD44780I2C : public CHD44780Base
{
public:
	CHD44780I2C(CI2CMaster* pI2CMaster, u8 nAddress = 0x27, u8 nColumns = 20, u8 nRows = 2);

	virtual void SetBacklightState(bool bEnabled) override;

protected:
	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) override;

	CI2CMaster* m_pI2CMaster;
	u8 m_nAddress;
};

#endif
