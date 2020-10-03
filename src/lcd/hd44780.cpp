//
// hd44780.cpp
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

#include <cmath>

#include <circle/logger.h>
#include <circle/sched/scheduler.h>

#include "lcd/hd44780.h"

// Custom characters for drawing bar graphs
// Empty (0x20) and full blocks (0xFF) already exist in the character set
const u8 CHD44780Base::CustomCharData[7][8] =
{
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	}
};

// Use ASCII space for empty bar, custom chars for 1-7 rows, 0xFF for full bar
const char CHD44780Base::BarChars[] = { ' ', '\x1', '\x2', '\x3', '\x4', '\x5', '\x6', '\x7', '\xff' };

CHD44780Base::CHD44780Base(u8 nColumns, u8 nRows)
	: CMT32LCD(),
	  mScheduler(CScheduler::Get()),
	  mRows(nRows),
	  mColumns(nColumns)
{
}

void CHD44780Base::WriteByte(u8 nByte, WriteMode Mode)
{
	WriteNybble(nByte >> 4, Mode);
	WriteNybble(nByte, Mode);
}

void CHD44780Base::WriteCommand(u8 nByte)
{
	// RS = LOW for command mode
	WriteByte(nByte, WriteMode::Command);
}

void CHD44780Base::WriteData(u8 nByte)
{
	// RS = HIGH for data mode
	WriteByte(nByte, WriteMode::Data);
}

void CHD44780Base::WriteData(const u8* pBytes, size_t nSize)
{
	for (size_t i = 0; i < nSize; ++i)
		WriteData(pBytes[i]);
}

void CHD44780Base::SetCustomChar(u8 nIndex, const u8 nCharData[8])
{
	assert(nIndex < 8);
	WriteCommand(0x40 | (nIndex << 3));

	for (u8 i = 0; i < 8; ++i)
		WriteData(nCharData[i]);
}

bool CHD44780Base::Initialize()
{
	assert(mScheduler != nullptr);

	// Validate dimensions - only 20x2 and 20x4 supported for now
	if (!(mRows == 2 || mRows == 4) || mColumns != 20)
		return false;

	// Give the LCD some time to start up
	mScheduler->MsSleep(50);

	// The following algorithm ensures the LCD is in the correct mode no matter what state it's currently in:
	// https://en.wikipedia.org/wiki/Hitachi_HD44780_LCD_controller#Mode_selection
	WriteNybble(0b0011, WriteMode::Command);
	mScheduler->MsSleep(50);
	WriteNybble(0b0011, WriteMode::Command);
	mScheduler->MsSleep(50);
	WriteNybble(0b0011, WriteMode::Command);
	mScheduler->MsSleep(50);

	// Switch to 4-bit mode
	WriteNybble(0b0010, WriteMode::Command);
	mScheduler->MsSleep(50);

	// Turn off
	WriteCommand(0b1000);

	// Clear display
	WriteCommand(0b0001);
	mScheduler->MsSleep(50);

	// Home cursor
	WriteCommand(0b0010);
	mScheduler->MsSleep(2);

	// Function set (4-bit, 2-line)
	WriteCommand(0b101000);

	// Set entry
	WriteCommand(0b0110);

	// Set custom characters
	for (size_t i = 0; i < sizeof(CustomCharData) / sizeof(*CustomCharData); ++i)
		SetCustomChar(i + 1, CustomCharData[i]);

	// Turn on
	WriteCommand(0b1100);

	return true;
}

void CHD44780Base::Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
	static u8 rowOffset[] = { 0, u8(0x40), mColumns, u8(0x40 + mColumns) };
	WriteCommand(0x80 | rowOffset[nCursorY] + nCursorX);

	const char* p = pText;
	while (*p)
		WriteData(*p++);

	if (bClearLine)
	{
		while ((p++ - pText) < (mColumns - nCursorX))
			WriteData(' ');
	}
}

void CHD44780Base::Clear()
{
	WriteCommand(0b0001);
	mScheduler->MsSleep(50);
}

void CHD44780Base::DrawPartLevelsSingle(u8 nRow)
{
	char lineBuf[18 + 1];

	for (u8 i = 0; i < 9; ++i)
	{
		lineBuf[i * 2] = BarChars[mPartLevels[i] / 2];
		lineBuf[i * 2 + 1] = ' ';
	}

	lineBuf[18] = '\0';

	Print(lineBuf, 0, nRow, true);
}

void CHD44780Base::DrawPartLevelsDouble(u8 nFirstRow)
{
	char line1Buf[18 + 1];
	char line2Buf[18 + 1];

	for (u8 i = 0; i < 9; ++i)
	{
		if (mPartLevels[i] > 8)
		{
			line1Buf[i * 2] = BarChars[mPartLevels[i] - 8];
			line2Buf[i * 2] = BarChars[8];
		}
		else
		{
			line1Buf[i * 2] = BarChars[0];
			line2Buf[i * 2] = BarChars[mPartLevels[i]];
		}

		line1Buf[i * 2 + 1] = line2Buf[i * 2 + 1] = ' ';
	}

	line1Buf[18] = line2Buf[18] = '\0';

	Print(line1Buf, 0, nFirstRow, true);
	Print(line2Buf, 0, nFirstRow + 1, true);
}

void CHD44780Base::Update(const CMT32SynthBase& Synth)
{
	CMT32LCD::Update(Synth);

	UpdatePartLevels(Synth);

	if (mRows == 2)
	{
		DrawPartLevelsSingle(0);
		Print(mTextBuffer, 0, 1, true);
	}
	else if (mRows == 4)
	{
		DrawPartLevelsDouble(0);
		Print(mTextBuffer, 0, 2, true);
	}
}
