//
// ui.h
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

#ifndef _ui_h
#define _ui_h

#include <circle/types.h>

#include "lcd/barchars.h"
#include "lcd/lcd.h"

class CSynthBase;

class CUserInterface
{
public:
	enum class TSysExDisplayMessage
	{
		Roland,
		Yamaha,
	};

	CUserInterface();

	void Update(CLCD& LCD, CSynthBase& Synth, unsigned int nTicks);

	void ShowSystemMessage(const char* pMessage, bool bSpinner = false);
	void ClearSpinnerMessage();
	void DisplayImage(TImage Image);
	void ShowSysExText(TSysExDisplayMessage Type, const u8* pMessage, size_t nSize, u8 nOffset);
	void ShowSysExBitmap(TSysExDisplayMessage Type, const u8* pData, size_t nSize);
	void EnterPowerSavingMode();
	void ExitPowerSavingMode();

	bool IsScrolling() const { return m_bIsScrolling; }

	static u8 CenterMessageOffset(CLCD& LCD, const char* pMessage);
	static void DrawChannelLevels(CLCD& LCD, u8 nBarHeight, float* pChannelLevels, float* pPeakLevels, u8 nChannels, bool bDrawBarBases);

private:
	enum class TState
	{
		None,
		DisplayingMessage,
		DisplayingSpinnerMessage,
		DisplayingImage,
		DisplayingSysExText,
		DisplayingSysExBitmap,
		EnteringPowerSavingMode,
		InPowerSavingMode
	};

	bool UpdateScroll(CLCD& LCD, unsigned int nTicks);
	bool DrawSystemState(CLCD& LCD) const;
	void DrawSysExText(CLCD& LCD, u8 nFirstRow) const;
	void DrawSysExBitmap(CLCD& LCD, u8 nFirstRow, u8 nRows) const;

	static void DrawChannelLevelsCharacter(CLCD& LCD, u8 nRows, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarSpacing, const float* pChannelLevels, u8 nChannels, bool bDrawBarBases);
	static void DrawChannelLevelsGraphical(CLCD& LCD, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarWidth, u8 nBarHeight, u8 nBarSpacing, const float* pChannelLevels, const float* pPeakLevels, u8 nChannels, bool bDrawBarBases);

	static constexpr size_t SystemMessageTextBufferSize = 256;
	static constexpr size_t SyxExTextBufferSize = 32 + 1;
	static constexpr size_t SysExPixelBufferSize = 64;

	static constexpr unsigned SystemMessageDisplayTimeMillis = 3000;
	static constexpr unsigned SystemMessageSpinnerTimeMillis = 32;
	static constexpr unsigned SC55DisplayTimeMillis = 3000;

	// UI state
	TState m_State;
	unsigned m_nStateTime;
	bool m_bIsScrolling;
	size_t m_nCurrentScrollOffset;
	size_t m_nCurrentSpinnerChar;
	TImage m_CurrentImage;
	char m_SystemMessageTextBuffer[SystemMessageTextBufferSize];
	TSysExDisplayMessage m_SysExDisplayMessageType;
	char m_SysExTextBuffer[SyxExTextBufferSize];
	u8 m_SysExPixelBuffer[SysExPixelBufferSize];
};

#endif
