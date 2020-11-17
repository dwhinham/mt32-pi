//
// synthlcd.cpp
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

#include "lcd/synthlcd.h"
#include "synth/mt32synth.h"
#include "utility.h"

CSynthLCD::CSynthLCD()
	: m_SystemState(TSystemState::None),
	  m_nSystemStateTime(0),
	  m_SystemMessageTextBuffer{'\0'},

	  m_MT32State(TMT32State::DisplayingPartStates),
	  m_nMT32StateTime(0),
	  m_MT32TextBuffer{'\0'},
	  m_nPreviousMasterVolume(0),
	  m_PartLevels{0},
	  m_PeakLevels{0},
	  m_PeakTimes{0}
{
}

void CSynthLCD::OnSystemMessage(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), pMessage);

	m_SystemState = TSystemState::DisplayingMessage;
	m_nSystemStateTime = ticks;
}

void CSynthLCD::OnMT32Message(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(m_MT32TextBuffer, sizeof(m_MT32TextBuffer), pMessage);

	m_MT32State = TMT32State::DisplayingMessage;
	m_nMT32StateTime = ticks;
}

void CSynthLCD::OnProgramChanged(u8 nPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	// Bail out if displaying an MT-32 message and it hasn't been on-screen long enough
	if (m_MT32State == TMT32State::DisplayingMessage && (ticks - m_nMT32StateTime) <= MSEC2HZ(MT32MessageDisplayTimeMillis))
		return;

	snprintf(m_MT32TextBuffer, sizeof(m_MT32TextBuffer), "%d|%s%s", nPartNum + 1, pSoundGroupName, pPatchName);

	m_MT32State = TMT32State::DisplayingTimbreName;
	m_nMT32StateTime = ticks;
}

void CSynthLCD::Update(const CMT32Synth& Synth)
{
	unsigned ticks = CTimer::Get()->GetTicks();
	u8 masterVolume = Synth.GetMasterVolume();

	// System message timeout
	if (m_SystemState == TSystemState::DisplayingMessage && (ticks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = ticks;
	}

	// Hide message if master volume changed and message has been displayed long enough
	if (m_nPreviousMasterVolume != masterVolume)
	{
		if (m_MT32State != TMT32State::DisplayingMessage || (ticks - m_nMT32StateTime) > MSEC2HZ(MT32MessageDisplayTimeMillis))
		{
			m_nPreviousMasterVolume = masterVolume;
			m_MT32State = TMT32State::DisplayingPartStates;
			m_nMT32StateTime = ticks;
		}
	}

	// Timbre change timeout
	if (m_MT32State == TMT32State::DisplayingTimbreName && (ticks - m_nMT32StateTime) > MSEC2HZ(TimbreDisplayTimeMillis))
	{
		m_MT32State = TMT32State::DisplayingPartStates;
		m_nMT32StateTime = ticks;
	}

	if (m_MT32State == TMT32State::DisplayingPartStates)
		UpdatePartStateText(Synth);
}

void CSynthLCD::UpdatePartStateText(const CMT32Synth& Synth)
{
	u32 partStates = Synth.GetPartStates();

	// First 5 parts
	for (u8 i = 0; i < 5; ++i)
	{
		bool state = (partStates >> i) & 1;
		m_MT32TextBuffer[i * 2] = state ? '\xff' : ('1' + i);
		m_MT32TextBuffer[i * 2 + 1] = ' ';
	}

	// Rhythm
	m_MT32TextBuffer[10] = (partStates >> 8) ? '\xff' : 'R';
	m_MT32TextBuffer[11] = ' ';

	// Volume
	sprintf(m_MT32TextBuffer + 12, "|vol:%3d", Synth.GetMasterVolume());
}

void CSynthLCD::UpdatePartLevels(const CMT32Synth& Synth)
{
	u32 partStates = Synth.GetPartStates();
	for (u8 i = 0; i < 9; ++i)
	{
		// If part active
		if ((partStates >> i) & 1)
		{
			// MIDI velocity range [0-127] to normalized range [0-1]
			float partLevel = Synth.GetVelocityForPart(i) / 127.0f;

			if (partLevel >= m_PartLevels[i])
				m_PartLevels[i] = partLevel;
		}
		else
			m_PartLevels[i] = Utility::Max(m_PartLevels[i] - BarFalloff, 0.0f);
	}
}

void CSynthLCD::UpdatePeakLevels()
{
	for (u8 i = 0; i < 9; ++i)
	{
		if (m_PartLevels[i] >= m_PeakLevels[i])
		{
			m_PeakLevels[i] = m_PartLevels[i];
			m_PeakTimes[i] = 100;
		}

		if (m_PeakTimes[i] == 0 && m_PeakLevels[i] > 0.0f)
			m_PeakLevels[i] = Utility::Max(m_PeakLevels[i] - PeakFalloff, 0.0f);
		else
			--m_PeakTimes[i];
	}
}
