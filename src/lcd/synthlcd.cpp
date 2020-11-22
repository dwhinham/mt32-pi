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

#include "lcd/synthlcd.h"
#include "synth/mt32synth.h"
#include "synth/soundfontsynth.h"
#include "utility.h"

CSynthLCD::CSynthLCD()
	: m_SystemState(TSystemState::None),
	  m_nSystemStateTime(0),
	  m_SystemMessageTextBuffer{'\0'},

	  m_MT32State(TMT32State::DisplayingPartStates),
	  m_nMT32StateTime(0),
	  m_MT32TextBuffer{'\0'},
	  m_nPreviousMasterVolume(0),
	  m_ChannelVelocities{0},
	  m_ChannelLevels{0},
	  m_ChannelPeakLevels{0},
	  m_ChannelPeakTimes{0}
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

void CSynthLCD::Update(CMT32Synth& Synth)
{
	unsigned ticks = CTimer::Get()->GetTicks();
	UpdateSystem(ticks);

	u8 masterVolume = Synth.GetMasterVolume();

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

void CSynthLCD::Update(CSoundFontSynth& Synth)
{
	unsigned ticks = CTimer::Get()->GetTicks();
	UpdateSystem(ticks);
}

void CSynthLCD::UpdateSystem(unsigned int nTicks)
{
	// System message timeout
	if (m_SystemState == TSystemState::DisplayingMessage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = nTicks;
	}
}

void CSynthLCD::UpdatePartStateText(const CMT32Synth& Synth)
{
	// First 5 parts
	for (u8 i = 0; i < 5; ++i)
	{
		bool bState = m_ChannelVelocities[i] > 0;
		m_MT32TextBuffer[i * 2] = bState ? '\xFF' : ('1' + i);
		m_MT32TextBuffer[i * 2 + 1] = ' ';
	}

	// Rhythm
	bool bState = m_ChannelVelocities[8] > 0;
	m_MT32TextBuffer[10] = bState ? '\xFF' : 'R';
	m_MT32TextBuffer[11] = ' ';

	// Volume
	sprintf(m_MT32TextBuffer + 12, "|vol:%3d", Synth.GetMasterVolume());
}

void CSynthLCD::UpdateChannelLevels(CSynthBase& Synth)
{
	const u8 nChannelCount = Synth.GetChannelVelocities(m_ChannelVelocities, Utility::ArraySize(m_ChannelVelocities));

	for (u8 nChannel = 0; nChannel < nChannelCount; ++nChannel)
	{
		// MIDI velocity range [0-127] to normalized range [0-1]
		float nLevel = m_ChannelVelocities[nChannel] / 127.0f;

		if (nLevel >= m_ChannelLevels[nChannel])
			m_ChannelLevels[nChannel] = nLevel;
		else
			m_ChannelLevels[nChannel] = Utility::Max(m_ChannelLevels[nChannel] - BarFalloff, 0.0f);
	}
}

void CSynthLCD::UpdateChannelPeakLevels()
{
	for (u8 i = 0; i < MIDIChannelCount; ++i)
	{
		if (m_ChannelLevels[i] >= m_ChannelPeakLevels[i])
		{
			m_ChannelPeakLevels[i] = m_ChannelLevels[i];
			m_ChannelPeakTimes[i] = 100;
		}

		if (m_ChannelPeakTimes[i] == 0 && m_ChannelPeakLevels[i] > 0.0f)
			m_ChannelPeakLevels[i] = Utility::Max(m_ChannelPeakLevels[i] - PeakFalloff, 0.0f);
		else
			--m_ChannelPeakTimes[i];
	}
}
