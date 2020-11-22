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

#include <circle/logger.h>
#include <circle/timer.h>

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
	: CSynthLCD(),
	  m_nRows(nRows),
	  m_nColumns(nColumns)
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

bool CHD44780Base::Initialize()
{
	// Validate dimensions - only 20x2 and 20x4 supported for now
	if (!(m_nRows == 2 || m_nRows == 4) || m_nColumns != 20)
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
	for (size_t i = 0; i < sizeof(CustomCharData) / sizeof(*CustomCharData); ++i)
		SetCustomChar(i + 1, CustomCharData[i]);

	// Turn on
	WriteCommand(0b1100);

	return true;
}

void CHD44780Base::Print(const char* pText, u8 nCursorX, u8 nCursorY, bool bClearLine, bool bImmediate)
{
	static u8 rowOffset[] = { 0, u8(0x40), m_nColumns, u8(0x40 + m_nColumns) };
	WriteCommand(0x80 | rowOffset[nCursorY] + nCursorX);

	const char* p = pText;
	while (*p && (p - pText) < m_nColumns)
		WriteData(*p++);

	if (bClearLine)
	{
		while ((p++ - pText) < (m_nColumns - nCursorX))
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

void CHD44780Base::DrawChannelLevels(u8 nFirstRow, u8 nRows, u8 nBarXOffset, u8 nBarSpacing, u8 nChannels)
{
	char LineBuf[4][TextBufferLength];

	// Initialize with ASCII spaces, terminate each row with a null
	memset(LineBuf, ' ', sizeof(LineBuf));
	for (u8 i = 0; i < 4; ++i)
		LineBuf[i][20] = '\0';

	// For each channel
	for (u8 i = 0; i < nChannels; ++i)
	{
		const u8 nCharIndex = i + i * nBarSpacing + nBarXOffset;
		assert(nCharIndex < 20);

		const u8 nLevelPixels = static_cast<u8>(m_ChannelLevels[i] * nRows * 8);
		const u8 nFullRows    = nLevelPixels / 8;
		const u8 nRemainder   = nLevelPixels % 8;

		for (u8 j = 0; j < nFullRows; ++j)
			LineBuf[nRows - j - 1][nCharIndex] = BarChars[8];

		for (u8 j = nFullRows; j < nRows; ++j)
			LineBuf[nRows - j - 1][nCharIndex] = BarChars[0];

		if (nRemainder)
			LineBuf[nRows - nFullRows - 1][nCharIndex] = BarChars[nRemainder];
	}

	for (u8 i = 0; i < nRows; ++i)
		Print(LineBuf[i], 0, nFirstRow + i, true);
}

void CHD44780Base::Update(CMT32Synth& Synth)
{
	CSynthLCD::Update(Synth);

	UpdateChannelLevels(Synth);

	if (m_nRows == 2)
	{
		if (m_SystemState == TSystemState::DisplayingMessage)
			Print(m_SystemMessageTextBuffer, 0, 0, true);
		else
			DrawChannelLevels(0, 1, 0, 1, MT32ChannelCount);

		Print(m_MT32TextBuffer, 0, 1, true);
	}
	else if (m_nRows == 4)
	{
		if (m_SystemState == TSystemState::DisplayingMessage)
		{
			// Clear top line
			Print("", 0, 0, true);
			Print(m_SystemMessageTextBuffer, 0, 1, true);
			Print("", 0, 2, true);
		}
		else
			DrawChannelLevels(0, 3, 0, 1, MT32ChannelCount);

		Print(m_MT32TextBuffer, 0, 3, true);
	}
}

void CHD44780Base::Update(CSoundFontSynth& Synth)
{
	CSynthLCD::Update(Synth);

	UpdateChannelLevels(Synth);

	if (m_nRows == 2)
	{
		if (m_SystemState == TSystemState::DisplayingMessage)
		{
			Print(m_SystemMessageTextBuffer, 0, 0, true);
			Print("", 0, 1, true);
		}
		else
			DrawChannelLevels(0, m_nRows, 2, 0, MIDIChannelCount);
	}
	else if (m_nRows == 4)
	{
		if (m_SystemState == TSystemState::DisplayingMessage)
		{
			// Clear top line
			Print("", 0, 0, true);
			Print(m_SystemMessageTextBuffer, 0, 1, true);
			Print("", 0, 2, true);
			Print("", 0, 3, true);
		}
		else
			DrawChannelLevels(0, m_nRows, 2, 0, MIDIChannelCount);
	}
}
