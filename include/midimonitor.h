//
// midimonitor.h
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

#ifndef _midimonitor_h
#define _midimonitor_h

#include <circle/types.h>

#include "utility.h"

class CMIDIMonitor
{
public:
	CMIDIMonitor();

	void OnShortMessage(u32 nMessage);
	void GetChannelLevels(unsigned int nTicks, float* pOutLevels, float* pOutPeaks, u16 nPercussionBitMask = (1 << 9));
	void AllNotesOff();
	void ResetControllers(bool bIsResetAllControllers);

private:
	static constexpr u8 ChannelCount = 16;
	static constexpr u8 NoteCount = 127;

	static constexpr float AttackTimeMillis = 20.0f;
	static constexpr float DecayTimeMillis = 100.0f;
	static constexpr float SustainLevel = 0.8f;
	static constexpr float ReleaseTimeMillis = 150.0f;

	static constexpr float PeakHoldTimeMillis = 2000.0f;
	static constexpr float PeakFalloffTimeMillis = 1000.0f;

	struct TNoteState
	{
		unsigned int nNoteOnTime;
		unsigned int nNoteOffTime;
		u8 nVelocity;
		bool bDamperFlag;
	};

	struct TChannelState
	{
		u8 nVolume;
		u8 nExpression;
		u8 nPan;
		u8 nDamper;
		TNoteState Notes[NoteCount];
	};

	void ProcessCC(u8 nChannel, u8 nCC, u8 nValue, unsigned int nTicks);
	inline float ComputeEnvelope(unsigned int nTicks, const TNoteState& NoteState) const;
	inline float ComputePercussionEnvelope(unsigned int nTicks, const TNoteState& NoteState) const;

	TChannelState m_State[ChannelCount];
	float m_PeakLevels[ChannelCount];
	unsigned int m_PeakTimes[ChannelCount];
};

#endif
