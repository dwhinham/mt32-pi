//
// hd44780.h
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

#ifndef _hd44780_h
#define _hd44780_h

#include <circle/gpiopin.h>
#include <circle/i2cmaster.h>
#include <circle/types.h>

#include "lcd/clcd.h"
#include "lcd/mt32lcd.h"
#include "mt32synth.h"

class CHD44780Base : public CMT32LCD
{
public:
	CHD44780Base(u8 nColumns = 20, u8 nRows = 2);
	virtual ~CHD44780Base() = default;

	// CCharacterLCD
	virtual bool Initialize() override;
	virtual void Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine = false, bool bImmediate = true) override;
	virtual void Clear() override;

	// CMT32LCD
	virtual void Update(const CMT32SynthBase& Synth) override;

protected:
	enum class TWriteMode
	{
		Data,
		Command
	};

	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) = 0;
	void WriteByte(u8 nByte, TWriteMode Mode);

	void WriteCommand(u8 nByte);
	void WriteData(u8 nByte);
	void WriteData(const u8* pBytes, size_t nSize);

	void SetCustomChar(u8 nIndex, const u8 nCharData[8]);

	void DrawPartLevelsSingle(u8 nRow);
	void DrawPartLevelsDouble(u8 nFirstRow);

	CScheduler* m_pScheduler;
	u8 m_nRows;
	u8 m_nColumns;

	static const u8 CustomCharData[7][8];
	static const char BarChars[9];
};

class CHD44780FourBit : public CHD44780Base
{
public:
	CHD44780FourBit(u8 nColumns = 20, u8 nRows = 2);

protected:
	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) override;

	static constexpr u8 GPIO_PIN_RS = 10;
	static constexpr u8 GPIO_PIN_RW = 9;
	static constexpr u8 GPIO_PIN_EN = 11;
	static constexpr u8 GPIO_PIN_D4 = 0;
	static constexpr u8 GPIO_PIN_D5 = 5;
	static constexpr u8 GPIO_PIN_D6 = 6;
	static constexpr u8 GPIO_PIN_D7 = 13;

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

protected:
	virtual void WriteNybble(u8 nNybble, TWriteMode Mode) override;

	CI2CMaster* m_pI2CMaster;
	u8 m_nAddress;
};

#endif
