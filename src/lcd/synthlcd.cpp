//
// synthlcd.cpp
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

#include <circle/timer.h>

#include "lcd/synthlcd.h"
#include "synth/mt32synth.h"
#include "synth/soundfontsynth.h"
#include "utility.h"

constexpr u8 SpinnerChars[] = {'_', '_', '_', '-', '\'', '\'', '^', '^', '`', '`', '-', '_', '_', '_'};

CSynthLCD::CSynthLCD()
	: m_SystemState(TSystemState::None),
	  m_nSystemStateTime(0),
	  m_nCurrentSpinnerChar(0),
	  m_CurrentImage(TImage::None),
	  m_SystemMessageTextBuffer{'\0'},

	  m_MT32State(TMT32State::DisplayingPartStates),
	  m_nMT32StateTime(0),
	  m_MT32TextBuffer{'\0'},
	  m_nPreviousMasterVolume(0),

	  m_bSC55DisplayingText(false),
	  m_bSC55DisplayingDots(false),
	  m_nSC55DisplayTextTime(0),
	  m_nSC55DisplayDotsTime(0),
	  m_SC55TextBuffer{'\0'},
	  m_SC55PixelBuffer{0},

	  m_ChannelVelocities{0},
	  m_ChannelLevels{0},
	  m_ChannelPeakLevels{0},
	  m_ChannelPeakTimes{0}
{
}

void CSynthLCD::OnSystemMessage(const char* pMessage, bool bSpinner)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();

	if (bSpinner)
	{
		constexpr int nMaxMessageLen = SystemMessageTextBufferSize - 3;
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "%-*.*s %c", nMaxMessageLen, nMaxMessageLen, pMessage, SpinnerChars[0]);
		m_SystemState = TSystemState::DisplayingSpinnerMessage;
		m_nCurrentSpinnerChar = 0;
	}
	else
	{
		snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), pMessage);
		m_SystemState = TSystemState::DisplayingMessage;
	}

	m_nSystemStateTime = nTicks;
}

void CSynthLCD::ClearSpinnerMessage()
{
	m_SystemState = TSystemState::None;
	m_nCurrentSpinnerChar = 0;
}

void CSynthLCD::OnDisplayImage(TImage Image)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();
	m_CurrentImage = Image;
	m_SystemState = TSystemState::DisplayingImage;
	m_nSystemStateTime = nTicks;
}

void CSynthLCD::EnterPowerSavingMode()
{
	snprintf(m_SystemMessageTextBuffer, sizeof(m_SystemMessageTextBuffer), "Power saving mode");
	m_SystemState = TSystemState::EnteringPowerSavingMode;
	m_nSystemStateTime = CTimer::Get()->GetTicks();
}

void CSynthLCD::ExitPowerSavingMode()
{
	SetBacklightEnabled(true);
	m_SystemState = TSystemState::None;
}

void CSynthLCD::OnMT32Message(const char* pMessage)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();

	snprintf(m_MT32TextBuffer, sizeof(m_MT32TextBuffer), pMessage);

	m_MT32State = TMT32State::DisplayingMessage;
	m_nMT32StateTime = nTicks;
}

void CSynthLCD::OnProgramChanged(u8 nPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	const unsigned ticks = CTimer::Get()->GetTicks();

	// Bail out if displaying an MT-32 message and it hasn't been on-screen long enough
	if (m_MT32State == TMT32State::DisplayingMessage && (ticks - m_nMT32StateTime) <= MSEC2HZ(MT32MessageDisplayTimeMillis))
		return;

	snprintf(m_MT32TextBuffer, sizeof(m_MT32TextBuffer), "%d|%s%s", nPartNum + 1, pSoundGroupName, pPatchName);

	m_MT32State = TMT32State::DisplayingTimbreName;
	m_nMT32StateTime = ticks;
}

void CSynthLCD::OnSC55DisplayText(const char* pMessage)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	snprintf(m_SC55TextBuffer, sizeof(m_SC55TextBuffer), pMessage);

	m_bSC55DisplayingText = true;
	m_nSC55DisplayTextTime = ticks;
}

void CSynthLCD::OnSC55DisplayDots(const u8* pData)
{
	unsigned ticks = CTimer::Get()->GetTicks();

	memcpy(m_SC55PixelBuffer, pData, sizeof(m_SC55PixelBuffer));

	m_bSC55DisplayingDots = true;
	m_nSC55DisplayDotsTime = ticks;
}

void CSynthLCD::Update(CMT32Synth& Synth)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();
	UpdateSystem(nTicks);

	const u8 nMasterVolume = Synth.GetMasterVolume();

	// Hide message if master volume changed and message has been displayed long enough
	if (m_nPreviousMasterVolume != nMasterVolume)
	{
		if (m_MT32State != TMT32State::DisplayingMessage || (nTicks - m_nMT32StateTime) > MSEC2HZ(MT32MessageDisplayTimeMillis))
		{
			m_nPreviousMasterVolume = nMasterVolume;
			m_MT32State = TMT32State::DisplayingPartStates;
			m_nMT32StateTime = nTicks;
		}
	}

	// Timbre change timeout
	if (m_MT32State == TMT32State::DisplayingTimbreName && (nTicks - m_nMT32StateTime) > MSEC2HZ(TimbreDisplayTimeMillis))
	{
		m_MT32State = TMT32State::DisplayingPartStates;
		m_nMT32StateTime = nTicks;
	}

	if (m_MT32State == TMT32State::DisplayingPartStates)
		UpdatePartStateText(Synth);
}

void CSynthLCD::Update(CSoundFontSynth& Synth)
{
	const unsigned nTicks = CTimer::Get()->GetTicks();
	UpdateSystem(nTicks);

	// Displaying text timeout
	if (m_bSC55DisplayingText && (nTicks - m_nSC55DisplayTextTime) > MSEC2HZ(SC55DisplayTimeMillis))
		m_bSC55DisplayingText = false;

	// Displaying text timeout
	if (m_bSC55DisplayingDots && (nTicks - m_nSC55DisplayDotsTime) > MSEC2HZ(SC55DisplayTimeMillis))
		m_bSC55DisplayingDots = false;
}

void CSynthLCD::UpdateSystem(unsigned int nTicks)
{
	// System message timeout
	if (m_SystemState == TSystemState::DisplayingMessage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = nTicks;
	}

	// Spinner update
	else if (m_SystemState == TSystemState::DisplayingSpinnerMessage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageSpinnerTimeMillis))
	{
		m_nCurrentSpinnerChar = (m_nCurrentSpinnerChar + 1) % sizeof(SpinnerChars);
		m_SystemMessageTextBuffer[SystemMessageTextBufferSize - 2] = SpinnerChars[m_nCurrentSpinnerChar];
		m_nSystemStateTime = nTicks;
	}

	// Image display update
	else if (m_SystemState == TSystemState::DisplayingImage && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		m_SystemState = TSystemState::None;
		m_nSystemStateTime = nTicks;
	}

	// Power saving
	else if (m_SystemState == TSystemState::EnteringPowerSavingMode && (nTicks - m_nSystemStateTime) > MSEC2HZ(SystemMessageDisplayTimeMillis))
	{
		SetBacklightEnabled(false);
		m_SystemState = TSystemState::InPowerSavingMode;
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
