//
// synthlcd.cpp
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

#include <circle/timer.h>
#include <circle/util.h>

#include <cstdio>

#include "lcd/ui.h"
#include "synth/synthbase.h"
#include "utility.h"

constexpr u32 ScrollDelayMillis = 1500;
constexpr u32 ScrollRateMillis = 175;
constexpr u8 BarSpacingPixels = 2;
constexpr u8 SpinnerChars[] = {'_', '_', '_', '-', '\'', '\'', '^', '^', '`', '`', '-', '_', '_', '_'};

CUserInterface::CUserInterface()
	: m_State(TState::None),
	  m_nStateTime(0),
	  m_bIsScrolling(false),
	  m_nCurrentScrollOffset(0),
	  m_nCurrentSpinnerChar(0),
	  m_CurrentImage(TImage::None),
	  m_SystemMessageTextBuffer{'\0'},
	  m_SysExDisplayMessageType(TSysExDisplayMessage::Roland),
	  m_SysExTextBuffer{'\0'},
	  m_SysExPixelBuffer{0}
{
}

bool CUserInterface::UpdateScroll(CLCD& LCD, unsigned int nTicks)
{
	const unsigned int nDeltaTicks = nTicks - m_nStateTime;

	const char* pMessage;
	if (m_State == TState::DisplayingMessage)
		pMessage = m_SystemMessageTextBuffer;
	else if (m_State == TState::DisplayingSysExText && m_SysExDisplayMessageType == TSysExDisplayMessage::Roland)
		pMessage = m_SysExTextBuffer;
	else
		return false;

	// TODO: API for getting width in pixels/characters for a string
	const size_t nCharWidth = LCD.GetType() == CLCD::TType::Graphical ? 20 : LCD.Width();
	const size_t nMessageLength = strlen(pMessage + m_nCurrentScrollOffset);

	if (nMessageLength <= nCharWidth)
		return false;

	// Position 0; delay
	const u32 nTimeout = m_nCurrentScrollOffset == 0 ? Utility::MillisToTicks(ScrollDelayMillis) : Utility::MillisToTicks(ScrollRateMillis);
	if (nDeltaTicks >= nTimeout)
	{
		++m_nCurrentScrollOffset;
		m_nStateTime = nTicks;
	}

	return true;
}

void CUserInterface::Update(CLCD& LCD, CSynthBase& Synth, unsigned int nTicks)
{
	// Update message scrolling
	m_bIsScrolling = UpdateScroll(LCD, nTicks);

	const unsigned int nDeltaTicks = nTicks - m_nStateTime;

	// System message timeout
	if (m_State == TState::DisplayingMessage && !m_bIsScrolling && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		m_State = TState::None;
		m_nStateTime = nTicks;
	}

	// Spinner update
	else if (m_State == TState::DisplayingSpinnerMessage && !m_bIsScrolling && nDeltaTicks >= Utility::MillisToTicks(SystemMessageSpinnerTimeMillis))
	{
		// TODO: API for getting width in pixels/characters for a string
		const size_t nCharWidth = LCD.GetType() == CLCD::TType::Graphical ? 20 : LCD.Width();

		m_nCurrentSpinnerChar = (m_nCurrentSpinnerChar + 1) % sizeof(SpinnerChars);
		m_SystemMessageTextBuffer[nCharWidth - 2] = SpinnerChars[m_nCurrentSpinnerChar];
		m_nStateTime = nTicks;
	}

	// Image display update
	else if (m_State == TState::DisplayingImage && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		m_State = TState::None;
		m_nStateTime = nTicks;
	}

	// SC-55 text timeout
	else if ((m_State == TState::DisplayingSysExText && !m_bIsScrolling || m_State == TState::DisplayingSysExBitmap) && nDeltaTicks >= Utility::MillisToTicks(SC55DisplayTimeMillis))
	{
		m_State = TState::None;
		m_nStateTime = nTicks;
	}

	// Power saving
	else if (m_State == TState::EnteringPowerSavingMode && nDeltaTicks >= Utility::MillisToTicks(SystemMessageDisplayTimeMillis))
	{
		LCD.SetBacklightState(false);
		m_State = TState::InPowerSavingMode;
		m_nStateTime = nTicks;
	}

	// Re-enable backlight
	if (m_State != TState::InPowerSavingMode && !LCD.GetBacklightState())
		LCD.SetBacklightState(true);

	// Power saving mode: no-op
	if (m_State == TState::InPowerSavingMode)
		return;

	LCD.Clear(false);

	// Draw synth UI if no drawable system state
	if (!DrawSystemState(LCD))
		Synth.UpdateLCD(LCD, nTicks);

	LCD.Flip();
}

void CUserInterface::ShowSystemMessage(const char* pMessage, bool bSpinner)
{
	const unsigned nTicks = CTimer::GetClockTicks();

	if (bSpinner)
	{
		constexpr int nMaxMessageLen = SystemMessageTextBufferSize - 3;
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "%-*.*s %c", nMaxMessageLen, nMaxMessageLen, pMessage, SpinnerChars[0]);
		m_State = TState::DisplayingSpinnerMessage;
		m_nCurrentSpinnerChar = 0;
	}
	else
	{
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), pMessage);
		m_State = TState::DisplayingMessage;
	}

	m_nCurrentScrollOffset = 0;
	m_nStateTime = nTicks;
}

void CUserInterface::ClearSpinnerMessage()
{
	m_State = TState::None;
	m_nCurrentSpinnerChar = 0;
}

void CUserInterface::DisplayImage(TImage Image)
{
	const unsigned nTicks = CTimer::GetClockTicks();
	m_CurrentImage = Image;
	m_State = TState::DisplayingImage;
	m_nStateTime = nTicks;
}

void CUserInterface::ShowSysExText(TSysExDisplayMessage Type, const u8* pMessage, size_t nSize, u8 nOffset)
{
	if (nOffset + nSize > SyxExTextBufferSize - 1)
		nSize = SyxExTextBufferSize - 1 - nOffset;

	memset(m_SysExTextBuffer, ' ', nOffset);
	memcpy(m_SysExTextBuffer + nOffset, pMessage, nSize);
	m_SysExTextBuffer[nOffset + nSize] = '\0';

	const unsigned nTicks = CTimer::GetClockTicks();
	m_SysExDisplayMessageType = Type;
	m_State = TState::DisplayingSysExText;
	m_nCurrentScrollOffset = 0;
	m_nStateTime = nTicks;
}

void CUserInterface::ShowSysExBitmap(TSysExDisplayMessage Type, const u8* pData, size_t nSize)
{
	if (nSize == 0)
		return;
	if (Type == TSysExDisplayMessage::Roland && nSize > 64)
		nSize = 64;
	else if (Type == TSysExDisplayMessage::Yamaha && nSize > 48)
		nSize = 48;

	const unsigned nTicks = CTimer::GetClockTicks();
	m_SysExDisplayMessageType = Type;
	memcpy(m_SysExPixelBuffer, pData, nSize);
	m_State = TState::DisplayingSysExBitmap;
	m_nStateTime = nTicks;
}

void CUserInterface::EnterPowerSavingMode()
{
	const unsigned nTicks = CTimer::GetClockTicks();
	snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "Power saving mode");
	m_State = TState::EnteringPowerSavingMode;
	m_nStateTime = nTicks;
}

void CUserInterface::ExitPowerSavingMode()
{
	m_State = TState::None;
}

u8 CUserInterface::CenterMessageOffset(CLCD& LCD, const char* pMessage)
{
	// TODO: API for getting width in pixels/characters for a string
	const u8 nCharWidth = LCD.GetType() == CLCD::TType::Graphical ? 20 : LCD.Width();
	const size_t nMessageLength = strlen(pMessage);
	return nMessageLength >= nCharWidth ? 0 : (nCharWidth - nMessageLength) / 2;
}

void CUserInterface::DrawChannelLevels(CLCD& LCD, u8 nBarHeight, float* pChannelLevels, float* pPeakLevels, u8 nChannels, bool bDrawBarBases)
{
	if (LCD.GetType() == CLCD::TType::Character)
	{
		const u8 nBarSpacing = LCD.Width() / nChannels / 2;
		const u8 nBarOffsetX = (LCD.Width() - nChannels - nChannels * nBarSpacing) / 2;
		DrawChannelLevelsCharacter(LCD, nBarHeight, nBarOffsetX, 0, nBarSpacing, pChannelLevels, nChannels, bDrawBarBases);
	}
	else
	{
		const u8 nTotalBarSpacing = (nChannels - 1) * BarSpacingPixels;
		const u8 nBarWidth = (LCD.Width() - nTotalBarSpacing) / nChannels;
		const u8 nTotalBarWidth = nBarWidth * nChannels;
		const u8 nBarOffsetX = (LCD.Width() - nTotalBarWidth - nTotalBarSpacing) / 2;
		DrawChannelLevelsGraphical(LCD, nBarOffsetX, 0, nBarWidth, nBarHeight, BarSpacingPixels, pChannelLevels, pPeakLevels, nChannels, bDrawBarBases);
	}
}

void CUserInterface::DrawChannelLevelsCharacter(CLCD& LCD, u8 nRows, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarSpacing, const float* pChannelLevels, u8 nChannels, bool bDrawBarBases)
{
	const u8 nWidth = LCD.Width();
	char LineBuf[nRows][nWidth + 1];
	const u8 nBarHeight = nRows * 8;

	// Initialize with ASCII spaces
	memset(LineBuf, ' ', sizeof(LineBuf));

	// For each channel
	for (u8 nChannel = 0; nChannel < nChannels; ++nChannel)
	{
		const u8 nPosX = nChannel + nChannel * nBarSpacing + nBarOffsetX;
		assert(nPosX < nWidth);

		u8 nLevelPixels = pChannelLevels[nChannel] * nBarHeight;
		if (bDrawBarBases && nLevelPixels == 0)
			nLevelPixels = 1;

		const u8 nFullRows  = nLevelPixels / 8;
		const u8 nRemainder = nLevelPixels % 8;

		for (u8 j = 0; j < nFullRows; ++j)
			LineBuf[nRows - j - 1][nPosX] = BarChars[8];

		for (u8 j = nFullRows; j < nRows; ++j)
			LineBuf[nRows - j - 1][nPosX] = BarChars[0];

		if (nRemainder)
			LineBuf[nRows - nFullRows - 1][nPosX] = BarChars[nRemainder];
	}

	for (u8 nRow = 0; nRow < nRows; ++nRow)
	{
		LineBuf[nRow][nWidth] = '\0';
		LCD.Print(LineBuf[nRow], 0, nBarYOffset + nRow, false, true);
	}
}

void CUserInterface::DrawChannelLevelsGraphical(CLCD& LCD, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarWidth, u8 nBarHeight, u8 nBarSpacing, const float* pChannelLevels, const float* pPeakLevels, u8 nChannels, bool bDrawBarBases)
{
	assert(pChannelLevels != nullptr);
	const u8 nBarMaxY = nBarHeight - 1;

	for (u8 nChannel = 0; nChannel < nChannels; ++nChannel)
	{
		const u8 nLevelPixels = pChannelLevels[nChannel] * nBarMaxY;
		const u8 nX1 = nBarOffsetX + nChannel * (nBarWidth + nBarSpacing);
		const u8 nX2 = nX1 + nBarWidth - 1;

		if (nLevelPixels > 0 || bDrawBarBases)
		{
			const u8 nY1 = nBarYOffset + (nBarMaxY - nLevelPixels);
			const u8 nY2 = nY1 + nLevelPixels;
			LCD.DrawFilledRect(nX1, nY1, nX2, nY2);
		}

		if (pPeakLevels)
		{
			const u8 nPeakLevelPixels = pPeakLevels[nChannel] * nBarMaxY;
			if (nPeakLevelPixels)
			{
				// TODO: DrawLine
				const u8 nY = nBarYOffset + (nBarMaxY - nPeakLevelPixels);
				LCD.DrawFilledRect(nX1, nY, nX2, nY);
			}
		}
	}
}

bool CUserInterface::DrawSystemState(CLCD& LCD) const
{
	// Nothing to do
	if (m_State == TState::None)
		return false;

	const u8 nHeight = LCD.Height();

	if (LCD.GetType() == CLCD::TType::Graphical)
	{
		const u8 nMessageRow = nHeight == 32 ? 0 : 1;

		if (m_State == TState::DisplayingImage)
			LCD.DrawImage(m_CurrentImage);
		else if (m_State == TState::DisplayingSysExBitmap)
			DrawSysExBitmap(LCD, 0, nHeight / 8);
		else if (m_State == TState::DisplayingSysExText)
			DrawSysExText(LCD, nMessageRow);
		else
		{
			const u8 nOffsetX = CenterMessageOffset(LCD, m_SystemMessageTextBuffer);
			LCD.Print(m_SystemMessageTextBuffer + m_nCurrentScrollOffset, nOffsetX, nMessageRow, true, false);
		}
	}
	else
	{
		// Character LCD can't display graphics
		if (m_State == TState::DisplayingImage || m_State == TState::DisplayingSysExBitmap)
			return false;

		if (m_State == TState::DisplayingSysExText)
			DrawSysExText(LCD, nHeight == 2 ? 0 : 1);
		else
		{
			const u8 nOffsetX = CenterMessageOffset(LCD, m_SystemMessageTextBuffer);

			if (nHeight == 2)
			{
				LCD.Print(m_SystemMessageTextBuffer + m_nCurrentScrollOffset, nOffsetX, 0, true);
				LCD.Print("", 0, 1, true);
			}
			else if (nHeight == 4)
			{
				// Clear top line
				LCD.Print("", 0, 0, true);
				LCD.Print(m_SystemMessageTextBuffer + m_nCurrentScrollOffset, nOffsetX, 1, true);
				LCD.Print("", 0, 2, true);
				LCD.Print("", 0, 3, true);
			}
		}
	}

	return true;
}

void CUserInterface::DrawSysExText(CLCD& LCD, u8 nFirstRow) const
{
	if (m_SysExDisplayMessageType == TSysExDisplayMessage::Roland)
	{
		// Roland SysEx text messages are single line and can be scrolled
		const u8 nOffsetX = CenterMessageOffset(LCD, m_SysExTextBuffer);
		LCD.Print(m_SysExTextBuffer + m_nCurrentScrollOffset, nOffsetX, nFirstRow, true, false);
	}
	else
	{
		// TODO: API for getting width in pixels/characters for a string
		const size_t nCharWidth = LCD.GetType() == CLCD::TType::Graphical ? 20 : LCD.Width();
		const u8 nOffsetX = (nCharWidth - 16) / 2;

		// Yamaha SysEx text messages are up to 16x2 characters and do not scroll, so split lines
		// and center on the LCD
		char Buffer[16 + 1];
		memcpy(Buffer, m_SysExTextBuffer, 16);
		Buffer[16] = '\0';

		LCD.Print(Buffer, nOffsetX, nFirstRow, true, false);

		if (strlen(m_SysExTextBuffer) > 16)
			LCD.Print(m_SysExTextBuffer + 16, nOffsetX, nFirstRow + 1, true, false);
	}
}

void CUserInterface::DrawSysExBitmap(CLCD& LCD, u8 nFirstRow, u8 nRows) const
{
	const u8 nWidth = LCD.Width();
	const u8 nHeight = LCD.Height();

	// Pixel data is 16x16, scale to 128x64 or 64x32 and center
	const u8 nScaleX      = nHeight == 64 ? 8 : 4;
	const u8 nScaleY      = nHeight == 64 ? 4 : 2;
	const size_t nOffsetX = (nWidth - 16 * nScaleX) / 2;
	const size_t nOffsetY = (nHeight - 16 * nScaleY) / 2;

	u8 nHeadLength = 0, nHeadPixels = 0, nTailPixels = 0;

	if (m_SysExDisplayMessageType == TSysExDisplayMessage::Roland)
	{
		// SC-55: Max 64 bytes; each byte representing 5 pixels (see p78 of SC-55 manual)
		// First 48 bytes have 5 columns of pixels, last 16 bytes have only 1
		nHeadLength = 48;
		nHeadPixels = 5;
		nTailPixels = 1;
	}
	else if (m_SysExDisplayMessageType == TSysExDisplayMessage::Yamaha)
	{
		// Yamaha: Max 48 bytes; each byte representing 7 pixels (see p16 of MU80 Sound List & MIDI Data book)
		// First 32 bytes have 7 columns of pixels, last 16 bytes have only 2
		nHeadLength = 32;
		nHeadPixels = 7;
		nTailPixels = 2;
	}

	for (u8 nByte = 0; nByte < sizeof(m_SysExPixelBuffer); ++nByte)
	{
		const u8 nPixels = nByte < nHeadLength ? nHeadPixels : nTailPixels;

		for (u8 nPixel = 0; nPixel < nPixels; ++nPixel)
		{
			const bool bPixelValue = (m_SysExPixelBuffer[nByte] >> (nHeadPixels - 1 - nPixel)) & 1;

			if (!bPixelValue)
				continue;

			const u8 nPosX = nByte / 16 * nHeadPixels + nPixel;
			const u8 nPosY = nByte % 16;

			const u8 nScaledX = nOffsetX + nPosX * nScaleX;
			const u8 nScaledY = nOffsetY + nPosY * nScaleY;

			LCD.DrawFilledRect(nScaledX, nScaledY, nScaledX + nScaleX - 1, nScaledY + nScaleY - 1);
		}
	}
}
