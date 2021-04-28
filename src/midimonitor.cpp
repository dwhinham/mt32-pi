//
// midimonitor.cpp
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

#include "midimonitor.h"

CMIDIMonitor::CMIDIMonitor()
	: m_PeakLevels{0.0f},
	  m_PeakTimes{0}
{
	memset(m_State, 0, sizeof(m_State));
	ResetControllers(false);
}

void CMIDIMonitor::OnShortMessage(u32 nMessage)
{
	const u8 nStatus  = nMessage & 0xF0;
	const u8 nChannel = nMessage & 0x0F;
	const u8 nData1   = (nMessage >> 8) & 0xFF;
	const u8 nData2   = (nMessage >> 16) & 0xFF;

	TChannelState& ChannelState = m_State[nChannel];
	TNoteState& NoteState = ChannelState.Notes[nData1];
	const unsigned int nTicks = CTimer::GetClockTicks();

	switch (nStatus)
	{
		// Note off
		case 0x80:
			if (!NoteState.bDamperFlag)
				NoteState.nNoteOffTime = nTicks;
			break;

		// Note on
		case 0x90:
			if (nData2)
			{
				NoteState.nNoteOnTime = nTicks;
				NoteState.nNoteOffTime = 0;
				NoteState.nVelocity = nData2;
				NoteState.bDamperFlag = ChannelState.nDamper;
			}
			else if (!NoteState.bDamperFlag)
				NoteState.nNoteOffTime = nTicks;
			break;

		// Control change
		case 0xB0:
			ProcessCC(nChannel, nData1, nData2, nTicks);
			break;

		// System Reset
		case 0xFF:
			AllNotesOff();
			ResetControllers(false);

		default:
			break;
	}
}

void CMIDIMonitor::GetChannelLevels(unsigned int nTicks, float* pOutLevels, float* pOutPeaks, u16 nPercussionBitMask)
{
	for (size_t nChannel = 0; nChannel < ChannelCount; ++nChannel)
	{
		const bool bIsPercussionChannel = nPercussionBitMask & (1 << nChannel);
		float nChannelVolume = 0.0f;

		for (size_t nNote = 0; nNote < NoteCount; ++nNote)
		{
			const TNoteState& NoteState = m_State[nChannel].Notes[nNote];
			const float nEnvelope = bIsPercussionChannel ? ComputePercussionEnvelope(nTicks, NoteState) : ComputeEnvelope(nTicks, NoteState);
			const float nNoteVolume = nEnvelope * (NoteState.nVelocity / 127.0f) * (m_State[nChannel].nVolume / 127.0f) * (m_State[nChannel].nExpression / 127.0f);
			nChannelVolume = Utility::Max(nChannelVolume, nNoteVolume);
		}

		nChannelVolume = Utility::Clamp(nChannelVolume, 0.0f, 1.0f);

		float nPeakLevel = m_PeakLevels[nChannel];
		const float nPeakUpdatedMillis = Utility::TicksToMillis(nTicks - m_PeakTimes[nChannel]);

		if (nPeakUpdatedMillis >= PeakHoldTimeMillis)
		{
			const float nPeakFallMillis = Utility::Max(nPeakUpdatedMillis - PeakHoldTimeMillis, 0.0f);
			nPeakLevel -= nPeakFallMillis / PeakFalloffTimeMillis;
			nPeakLevel = Utility::Clamp(nPeakLevel, 0.0f, 1.0f);
		}

		if (nChannelVolume >= nPeakLevel)
		{
			nPeakLevel = nChannelVolume;
			m_PeakLevels[nChannel] = nChannelVolume;
			m_PeakTimes[nChannel] = nTicks;
		}

		pOutLevels[nChannel] = nChannelVolume;
		pOutPeaks[nChannel] = nPeakLevel;
	}
}

void CMIDIMonitor::AllNotesOff()
{
	const unsigned int nTicks = CTimer::GetClockTicks();

	for (auto& Channel : m_State)
	{
		for (auto& Note : Channel.Notes)
		{
			if (Note.nNoteOnTime > Note.nNoteOffTime)
				Note.nNoteOffTime = nTicks;

			Note.bDamperFlag = false;
		}
	}
}

void CMIDIMonitor::ResetControllers(bool bIsResetAllControllers)
{
	for (auto& Channel : m_State)
	{
		Channel.nExpression = 127;
		Channel.nDamper = 0;

		// The MIDI specification says that certain controllers should not be reset
		// in response to a Reset All Controllers message
		if (!bIsResetAllControllers)
		{
			Channel.nVolume = 100;
			Channel.nPan = 64;
		}
	}
}

void CMIDIMonitor::ProcessCC(u8 nChannel, u8 nCC, u8 nValue, unsigned int nTicks)
{
	TChannelState& ChannelState = m_State[nChannel];

	switch (nCC)
	{
		// Channel volume
		case 0x07:
			ChannelState.nVolume = nValue;
			break;

		// Pan
		case 0x0A:
			ChannelState.nPan = nValue;
			break;

		// Expression
		case 0x0B:
			ChannelState.nExpression = nValue;
			break;

		// Damper pedal
		case 0x40:
			ChannelState.nDamper = nValue;

			// Damper released; trigger note-off for flagged notes
			if (!nValue)
			{
				for (auto& Note : ChannelState.Notes)
				{
					if (Note.bDamperFlag)
					{
						Note.nNoteOffTime = nTicks;
						Note.bDamperFlag = false;
					}
				}
			}
			break;

		// According to the MIDI spec, the following Channel Mode messages
		// all function as All Notes Off messages
		case 0x78:	// All Sound Off
		case 0x7B:	// All Notes Off
		case 0x7C:	// Omni Off
		case 0x7D:	// Omni On
		case 0x7E:	// Mono On
		case 0x7F:	// Mono Off
			AllNotesOff();
			break;

		// Reset All Controllers
		case 0x79:
			ResetControllers(true);
			break;

		default:
			break;
	}
}

float CMIDIMonitor::ComputeEnvelope(unsigned int nTicks, const TNoteState& NoteState) const
{
	if (NoteState.nNoteOnTime == 0)
		return 0.0f;

	nTicks = Utility::Max(nTicks, Utility::Max(NoteState.nNoteOnTime, NoteState.nNoteOffTime));

	// Note is on
	if (NoteState.nNoteOffTime == 0)
	{
		const float nNoteOnDurationMillis = Utility::TicksToMillis(nTicks - NoteState.nNoteOnTime);

		// Attack phase
		if (nNoteOnDurationMillis < AttackTimeMillis)
			return nNoteOnDurationMillis / AttackTimeMillis;

		// Decay phase
		else if (nNoteOnDurationMillis < AttackTimeMillis + DecayTimeMillis)
		{
			const float nDecayDurationMillis = nNoteOnDurationMillis - AttackTimeMillis;
			return 1.0f - (nDecayDurationMillis / DecayTimeMillis) * (1.0f - SustainLevel);
		}

		// Sustain phase
		return SustainLevel;
	}

	// Note has been released
	else
	{
		const float nGateDurationMillis = Utility::TicksToMillis(NoteState.nNoteOffTime - NoteState.nNoteOnTime);	
		float nVolume;

		// Note off during attack phase
		if (nGateDurationMillis < AttackTimeMillis)
			nVolume = nGateDurationMillis / AttackTimeMillis;

		// Note off during decay phase
		else if (nGateDurationMillis < AttackTimeMillis + DecayTimeMillis)
			nVolume = 1.0f - ((nGateDurationMillis - AttackTimeMillis) / DecayTimeMillis) * (1.0f - SustainLevel);
	
		// Note off during sustain phase
		else
			nVolume = SustainLevel;

		const float nNoteOffDurationMillis = Utility::TicksToMillis(nTicks - NoteState.nNoteOffTime);
		if (nNoteOffDurationMillis > ReleaseTimeMillis)
			return 0.0f;

		return nVolume - nNoteOffDurationMillis / ReleaseTimeMillis;
	}
}

float CMIDIMonitor::ComputePercussionEnvelope(unsigned int nTicks, const TNoteState& NoteState) const
{
	if (NoteState.nNoteOnTime == 0)
		return 0.0f;

	nTicks = Utility::Max(nTicks, NoteState.nNoteOnTime);

	const float nNoteOnDurationMillis = Utility::TicksToMillis(nTicks - NoteState.nNoteOnTime);

	if (nNoteOnDurationMillis > ReleaseTimeMillis)
		return 0.0f;

	// No decay/sustain for percussion
	return 1.0f - nNoteOnDurationMillis / ReleaseTimeMillis;
}
