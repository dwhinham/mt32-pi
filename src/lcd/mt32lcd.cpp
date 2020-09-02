//
// mt32lcd.cpp
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

#include <circle/timer.h>

#include <cmath>

#include "lcd/mt32lcd.h"
#include "mt32synth.h"

CMT32LCD::CMT32LCD()
 	: mState(State::DisplayingPartStates),
	  mTextBuffer{'\0'},
	  mPreviousMasterVolume(0),
	  mPartLevels{0},
	  mPeakLevels{0},
	  mPeakTimes{0}
{
}

void CMT32LCD::OnLCDMessage(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(mTextBuffer, sizeof(mTextBuffer), pMessage);

	mState = State::DisplayingMessage;
	mLCDStateTime = ticks;
}

void CMT32LCD::OnProgramChanged(u8 pPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	// Bail out if displaying a message and it hasn't been on-screen long enough
	if (mState == State::DisplayingMessage && (ticks - mLCDStateTime) <= MSEC2HZ(MessageDisplayTimeMillis))
		return;

	snprintf(mTextBuffer, sizeof(mTextBuffer), "%d|%s%s", pPartNum + 1, pSoundGroupName, pPatchName);

	mState = State::DisplayingTimbreName;
	mLCDStateTime = ticks;
}

void CMT32LCD::Update(const CMT32SynthBase& Synth)
{
	unsigned ticks = CTimer::Get()->GetTicks();
	u8 masterVolume = Synth.GetMasterVolume();

	// Hide message if master volume changed and message has been displayed long enough
	if (mPreviousMasterVolume != masterVolume)
	{
		if (mState != State::DisplayingMessage || (ticks - mLCDStateTime) > MSEC2HZ(MessageDisplayTimeMillis))
		{
			mPreviousMasterVolume = masterVolume;
			mState = State::DisplayingPartStates;
			mLCDStateTime = ticks;
		}
	}

	// Timbre change timeout
	if (mState == State::DisplayingTimbreName && (ticks - mLCDStateTime) > MSEC2HZ(TimbreDisplayTimeMillis))
	{
		mState = State::DisplayingPartStates;
		mLCDStateTime = ticks;
	}
	
	if (mState == State::DisplayingPartStates)
		UpdatePartStateText(pSynth);
}

void CMT32LCD::UpdatePartStateText(const CMT32SynthBase& pSynth)
{
	u32 partStates = pSynth.GetPartStates();

	// First 5 parts
	for (u8 i = 0; i < 5; ++i)
	{
		bool state = (partStates >> i) & 1;
		mTextBuffer[i * 2] = state ? '\xff' : ('1' + i);
		mTextBuffer[i * 2 + 1] = ' ';
	}

	// Rhythm
	mTextBuffer[10] = (partStates >> 8) ? '\xff' : 'R';
	mTextBuffer[11] = ' ';

	// Volume
	sprintf(mTextBuffer + 12, "|vol:%3d", pSynth.GetMasterVolume());
}

void CMT32LCD::UpdatePartLevels(const CMT32SynthBase& pSynth)
{
	u32 partStates = pSynth.GetPartStates();
	for (u8 i = 0; i < 9; ++i)
	{
		if ((partStates >> i) & 1)
			mPartLevels[i] = floor(VelocityScale * pSynth.GetVelocityForPart(i)) + 0.5f;
		else if (mPartLevels[i] > 0)
			--mPartLevels[i];
	}
}

void CMT32LCD::UpdatePeakLevels()
{
	for (u8 i = 0; i < 9; ++i)
	{
		if (mPartLevels[i] > mPeakLevels[i])
		{
			mPeakLevels[i] = mPartLevels[i];
			mPeakTimes[i] = 100;
		}
		else if (mPartLevels[i] > 0)
			--mPartLevels[i];

		if (mPeakTimes[i] == 0 && mPeakLevels[i] > 0)
		{
			--mPeakLevels[i];
			mPeakTimes[i] = 3;
		}
		else
			--mPeakTimes[i];
	}
}
