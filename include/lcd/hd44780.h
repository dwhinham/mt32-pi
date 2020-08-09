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
#include "mt32synth.h"

class CHD44780Base : public CCharacterLCD
{
public:
	CHD44780Base(u8 pColumns = 20, u8 pRows = 2);
	virtual ~CHD44780Base() = default;

	virtual bool Initialize() override;
	virtual void Print(const char* pText, u8 pCursorX, u8 pCursorY, bool pClearLine = false, bool pImmediate = true) override;
	virtual void Clear() override;
	virtual void SetMessage(const char* pMessage) override;
	virtual void ClearMessage() override;
	virtual void Update(CMT32SynthBase* pSynth = nullptr) override;

protected:
	enum class WriteMode
	{
		Data,
		Command
	};

	virtual void WriteNybble(u8 pNybble, WriteMode pMode) = 0;
	void WriteByte(u8 pByte, WriteMode pMode);

	void WriteCommand(u8 pByte);
	void WriteData(u8 pByte);
	void WriteData(const u8* pBytes, size_t pSize);

	void SetCustomChar(u8 pIndex, const u8 pCharData[8]);

	void DrawStatusLine(const CMT32SynthBase* pSynth, u8 pRow = 0);
	void UpdatePartLevels(const CMT32SynthBase* pSynth);
	void DrawPartLevelsSingle(u8 pRow);
	void DrawPartLevelsDouble(u8 pFirstRow);

	// MIDI velocity range [1-127] to bar graph height range [0-16] scale factor
	static constexpr float VelocityScale = 16.f / (127.f - 1.f);
	static constexpr size_t TextBufferLength = 20;

	CScheduler* mScheduler;
	u8 mRows;
	u8 mColumns;

	// MT-32 state
	bool mMessageFlag;
	char mMessageText[MT32Emu::SYSEX_BUFFER_SIZE + 1];
	u8 mPartLevels[9];

	static const u8 CustomCharData[7][8];
	static const char BarChars[9];
};

class CHD44780FourBit : public CHD44780Base
{
public:
	CHD44780FourBit(u8 pColumns = 20, u8 pRows = 2);

protected:
	virtual void WriteNybble(u8 pNybble, WriteMode pMode) override;

	static constexpr u8 GPIO_PIN_RS = 10;
	static constexpr u8 GPIO_PIN_RW = 9;
	static constexpr u8 GPIO_PIN_EN = 11;
	static constexpr u8 GPIO_PIN_D4 = 0;
	static constexpr u8 GPIO_PIN_D5 = 5;
	static constexpr u8 GPIO_PIN_D6 = 6;
	static constexpr u8 GPIO_PIN_D7 = 13;

	CGPIOPin mRS;
	CGPIOPin mRW;
	CGPIOPin mEN;
	CGPIOPin mD4;
	CGPIOPin mD5;
	CGPIOPin mD6;
	CGPIOPin mD7;
};

class CHD44780I2C : public CHD44780Base
{
public:
	CHD44780I2C(CI2CMaster* pI2CMaster, u8 pAddress = 0x27, u8 pColumns = 20, u8 pRows = 2);

protected:
	virtual void WriteNybble(u8 pNybble, WriteMode pMode) override;

	CI2CMaster* mI2CMaster;
	u8 mAddress;
};

#endif
