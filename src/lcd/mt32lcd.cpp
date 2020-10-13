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
 	: m_State(TState::DisplayingPartStates),
	  m_nStateTime(0),
	  m_TextBuffer{'\0'},
	  m_nPreviousMasterVolume(0),
	  m_PartLevels{0},
	  m_PeakLevels{0},
	  m_PeakTimes{0}
{
}

void CMT32LCD::OnLCDMessage(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(m_TextBuffer, sizeof(m_TextBuffer), pMessage);

	m_State = TState::DisplayingMessage;
	m_nStateTime = ticks;
}

void CMT32LCD::OnProgramChanged(u8 nPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	// Bail out if displaying a message and it hasn't been on-screen long enough
	if (m_State == TState::DisplayingMessage && (ticks - m_nStateTime) <= MSEC2HZ(MessageDisplayTimeMillis))
		return;

	snprintf(m_TextBuffer, sizeof(m_TextBuffer), "%d|%s%s", nPartNum + 1, pSoundGroupName, pPatchName);

	m_State = TState::DisplayingTimbreName;
	m_nStateTime = ticks;
}

void CMT32LCD::Update(const CMT32SynthBase& Synth)
{
#if 0
	unsigned ticks = CTimer::Get()->GetTicks();
	u8 masterVolume = Synth.GetMasterVolume();

	// Hide message if master volume changed and message has been displayed long enough
	if (m_nPreviousMasterVolume != masterVolume)
	{
		if (m_State != TState::DisplayingMessage || (ticks - m_nStateTime) > MSEC2HZ(MessageDisplayTimeMillis))
		{
			m_nPreviousMasterVolume = masterVolume;
			m_State = TState::DisplayingPartStates;
			m_nStateTime = ticks;
		}
	}

	// Timbre change timeout
	if (m_State == TState::DisplayingTimbreName && (ticks - m_nStateTime) > MSEC2HZ(TimbreDisplayTimeMillis))
	{
		m_State = TState::DisplayingPartStates;
		m_nStateTime = ticks;
	}

	if (m_State == TState::DisplayingPartStates)
		UpdatePartStateText(Synth);
#endif
}

void CMT32LCD::UpdatePartStateText(const CMT32SynthBase& Synth)
{
	u32 partStates = Synth.GetPartStates();

	// First 5 parts
	for (u8 i = 0; i < 5; ++i)
	{
		bool state = (partStates >> i) & 1;
		m_TextBuffer[i * 2] = state ? '\xff' : ('1' + i);
		m_TextBuffer[i * 2 + 1] = ' ';
	}

	// Rhythm
	m_TextBuffer[10] = (partStates >> 8) ? '\xff' : 'R';
	m_TextBuffer[11] = ' ';

	// Volume
	sprintf(m_TextBuffer + 12, "|vol:%3d", Synth.GetMasterVolume());
}

void CMT32LCD::UpdatePartLevels(const CMT32SynthBase& Synth)
{
	u32 partStates = Synth.GetPartStates();
	for (u8 i = 0; i < 9; ++i)
	{
		if ((partStates >> i) & 1)
			m_PartLevels[i] = floor(VelocityScale * Synth.GetVelocityForPart(i)) + 0.5f;
		else if (m_PartLevels[i] > 0)
			--m_PartLevels[i];
	}
}

void CMT32LCD::UpdatePeakLevels()
{
	for (u8 i = 0; i < 9; ++i)
	{
		if (m_PartLevels[i] > m_PeakLevels[i])
		{
			m_PeakLevels[i] = m_PartLevels[i];
			m_PeakTimes[i] = 100;
		}
		else if (m_PartLevels[i] > 0)
			--m_PartLevels[i];

		if (m_PeakTimes[i] == 0 && m_PeakLevels[i] > 0)
		{
			--m_PeakLevels[i];
			m_PeakTimes[i] = 3;
		}
		else
			--m_PeakTimes[i];
	}
}
