//
// mt32lcd.h
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

#ifndef _mt32lcd_h
#define _mt32lcd_h

#include <circle/types.h>

#include "lcd/clcd.h"

class CMT32SynthBase;

class CMT32LCD : public CCharacterLCD
{
public:
	enum class State
	{
		DisplayingPartStates,
		DisplayingTimbreName,
		DisplayingMessage,
	};

	CMT32LCD();

	void OnLCDMessage(const char* pMessage);
	void OnProgramChanged(u8 nPartNum, const char* pSoundGroupName, const char* pPatchName);

	virtual void Update(const CMT32SynthBase& Synth) = 0;

protected:
	void UpdatePartStateText(const CMT32SynthBase& Synth);
	void UpdatePartLevels(const CMT32SynthBase& Synth);
	void UpdatePeakLevels();

	// 20 characters plus null terminator
	static constexpr size_t TextBufferLength = 20 + 1;

	// MIDI velocity range [1-127] to bar graph height range [0-16] scale factor
	static constexpr float VelocityScale = 16.f / (127.f - 1.f);

	static constexpr unsigned MessageDisplayTimeMillis = 200; 
	static constexpr unsigned TimbreDisplayTimeMillis = 1200; 

	unsigned mLCDStateTime;
	State mState;

	// MT-32 state
	char mTextBuffer[TextBufferLength];
	u8 mPreviousMasterVolume;
	u8 mPartLevels[9];
	u8 mPeakLevels[9];
	u8 mPeakTimes[9];
};

#endif
