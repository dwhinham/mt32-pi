//
// hd44780.cpp
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

#include <circle/logger.h>
#include <circle/timer.h>

#include "lcd/barchars.h"
#include "lcd/drivers/hd44780.h"

CHD44780Base::CHD44780Base(u8 nColumns, u8 nRows)
	: CLCD(nColumns, nRows),
	  m_RowOffsets{ 0, 0x40, nColumns, u8(0x40 + nColumns) },
	  m_BarCharSet(TBarCharSet::None)
{
}

void CHD44780Base::WriteByte(u8 nByte, TWriteMode Mode)
{
	WriteNybble(nByte >> 4, Mode);
	WriteNybble(nByte, Mode);
}

void CHD44780Base::WriteCommand(u8 nByte)
{
	// RS = LOW for command mode
	WriteByte(nByte, TWriteMode::Command);
}

void CHD44780Base::WriteData(u8 nByte)
{
	// RS = HIGH for data mode
	WriteByte(nByte, TWriteMode::Data);
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

void CHD44780Base::SetBarChars(TBarCharSet CharSet)
{
	if (CharSet == m_BarCharSet)
		return;

	if (CharSet == TBarCharSet::Wide)
	{
		for (size_t i = 0; i < Utility::ArraySize(CustomBarCharDataWide); ++i)
			SetCustomChar(i, CustomBarCharDataWide[i]);
	}
	else
	{
		for (size_t i = 0; i < Utility::ArraySize(CustomBarCharDataNarrow); ++i)
			SetCustomChar(i, CustomBarCharDataNarrow[i]);
	}

	m_BarCharSet = CharSet;
}

bool CHD44780Base::Initialize()
{
	// Validate dimensions - only 16x2, 16x4, 20x2 and 20x4 supported for now
	if (!(m_nHeight == 2 || m_nHeight == 4) || !(m_nWidth == 16 || m_nWidth == 20))
		return false;

	// Give the LCD some time to start up
	CTimer::SimpleMsDelay(50);

	// The following algorithm ensures the LCD is in the correct mode no matter what state it's currently in:
	// https://en.wikipedia.org/wiki/Hitachi_HD44780_LCD_controller#Mode_selection
	WriteNybble(0b0011, TWriteMode::Command);
	CTimer::SimpleMsDelay(50);
	WriteNybble(0b0011, TWriteMode::Command);
	CTimer::SimpleMsDelay(50);
	WriteNybble(0b0011, TWriteMode::Command);
	CTimer::SimpleMsDelay(50);

	// Switch to 4-bit mode
	WriteNybble(0b0010, TWriteMode::Command);
	CTimer::SimpleMsDelay(50);

	// Turn off
	WriteCommand(0b1000);

	// Clear display
	WriteCommand(0b0001);
	CTimer::SimpleMsDelay(50);

	// Home cursor
	WriteCommand(0b0010);
	CTimer::SimpleMsDelay(2);

	// Function set (4-bit, 2-line)
	WriteCommand(0b101000);

	// Set entry
	WriteCommand(0b0110);

	// Set custom characters
	SetBarChars(TBarCharSet::Narrow);

	// Turn on
	WriteCommand(0b1100);

	return true;
}

void CHD44780Base::Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
	if (bClearLine)
	{
		WriteCommand(0x80 | m_RowOffsets[nCursorY]);
		for (u8 nChar = 0; nChar < nCursorX; ++nChar)
			WriteData(' ');
	}
	else
		WriteCommand(0x80 | m_RowOffsets[nCursorY] + nCursorX);

	const char* p = pText;
	while (*p && (p - pText) < (m_nWidth - nCursorX))
		WriteData(*p++);

	if (bClearLine)
	{
		while ((p++ - pText) < (m_nWidth - nCursorX))
			WriteData(' ');
	}
}

void CHD44780Base::Clear(bool bImmediate)
{
	if (!bImmediate)
		return;

	WriteCommand(0b0001);
	CTimer::SimpleMsDelay(50);
}
