//
// ui.h
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

#ifndef _ui_h
#define _ui_h

#include <circle/types.h>

#include "lcd/barchars.h"
#include "lcd/lcd.h"

class CSynthBase;

class CUserInterface
{
public:
	enum class TState
	{
		None,
		DisplayingMessage,
		DisplayingSpinnerMessage,
		DisplayingImage,
		DisplayingSC55Text,
		DisplayingSC55Dots,
		EnteringPowerSavingMode,
		InPowerSavingMode
	};

	CUserInterface();

	void Update(CLCD& LCD, CSynthBase& Synth, unsigned int nTicks);

	void ShowSystemMessage(const char* pMessage, bool bSpinner = false);
	void ClearSpinnerMessage();
	void DisplayImage(TImage Image);
	void ShowSC55Text(const char* pMessage);
	void ShowSC55Dots(const u8* pData);
	void EnterPowerSavingMode();
	void ExitPowerSavingMode();

	bool IsScrolling() const { return m_bIsScrolling; }

	static u8 CenterMessageOffset(CLCD& LCD, const char* pMessage);
	static void DrawChannelLevels(CLCD& LCD, u8 nBarHeight, float* pChannelLevels, float* pPeakLevels, u8 nChannels, bool bDrawBarBases);

private:
	bool UpdateScroll(CLCD& LCD, unsigned int nTicks);
	bool DrawSystemState(CLCD& LCD) const;
	void DrawSC55Dots(CLCD& LCD, u8 nFirstRow, u8 nRows) const;

	static void DrawChannelLevelsCharacter(CLCD& LCD, u8 nRows, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarSpacing, const float* pChannelLevels, u8 nChannels, bool bDrawBarBases);
	static void DrawChannelLevelsGraphical(CLCD& LCD, u8 nBarOffsetX, u8 nBarYOffset, u8 nBarWidth, u8 nBarHeight, u8 nBarSpacing, const float* pChannelLevels, const float* pPeakLevels, u8 nChannels, bool bDrawBarBases);

	static constexpr size_t SystemMessageTextBufferSize = 256;
	static constexpr size_t SC55TextBufferSize = 32 + 1;

	// 64 bytes; each byte representing 5 pixels (see p78 of SC-55 manual)
	static constexpr size_t SC55PixelBufferSize = 64;

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
	char m_SC55TextBuffer[SC55TextBufferSize];
	u8 m_SC55PixelBuffer[SC55PixelBufferSize];
};

#endif
