//
// oplsynth.cpp
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

#include <circle/logger.h>

#include "synth/oplsynth.h"

const char OPLSynthName[] = "oplsynth";

COPLSynth::COPLSynth(unsigned nSampleRate)
	: CSynthBase(nSampleRate),

	  m_pSynth(nullptr),
	  m_nVolume(100),
	  m_nCurrentBank(0)
{
}

COPLSynth::~COPLSynth()
{
	if (m_pSynth)
		adl_close(m_pSynth);
}

bool COPLSynth::Initialize()
{
	m_pSynth = adl_init(m_nSampleRate);

	if (!m_pSynth)
	{
		CLogger::Get()->Write(OPLSynthName, LogError, adl_errorString());
		return false;
	}

	adl_setNumChips(m_pSynth, 4);
	adl_switchEmulator(m_pSynth, ADLMIDI_EMU_DOSBOX);

	return true;
}

void COPLSynth::HandleMIDIShortMessage(u32 nMessage)
{
	const u8 nStatus  = nMessage & 0xFF;
	const u8 nChannel = nMessage & 0x0F;
	const u8 nData1   = (nMessage >> 8) & 0xFF;
	const u8 nData2   = (nMessage >> 16) & 0xFF;

	// Handle system real-time messages
	if (nStatus == 0xFF)
	{
		m_Lock.Acquire();
		adl_rt_resetState(m_pSynth);
		m_Lock.Release();
		return;
	}

	m_Lock.Acquire();

	// Handle channel messages
	switch (nStatus & 0xF0)
	{
		// Note off
		case 0x80:
			adl_rt_noteOff(m_pSynth, nChannel, nData1);
			break;

		// Note on
		case 0x90:
			adl_rt_noteOn(m_pSynth, nChannel, nData1, nData2);
			break;

		// Polyphonic key pressure/aftertouch
		case 0xA0:
			adl_rt_noteAfterTouch(m_pSynth, nChannel, nData1, nData2);
			break;

		// Control change
		case 0xB0:
			adl_rt_controllerChange(m_pSynth, nChannel, nData1, nData2);
			break;

		// Program change
		case 0xC0:
			adl_rt_patchChange(m_pSynth, nChannel, nData1);
			break;

		// Channel pressure/aftertouch
		case 0xD0:
			adl_rt_channelAfterTouch(m_pSynth, nChannel, nData1);
			break;

		// Pitch bend
		case 0xE0:
			adl_rt_pitchBendML(m_pSynth, nChannel, nData2, nData1);
			break;
	}

	m_Lock.Release();

	// Update MIDI monitor
	CSynthBase::HandleMIDIShortMessage(nMessage);
}

void COPLSynth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	m_Lock.Acquire();
	adl_rt_systemExclusive(m_pSynth, pData, nSize);
	m_Lock.Release();
}

bool COPLSynth::IsActive()
{
	// TODO
	return true;
}

void COPLSynth::AllSoundOff()
{
	m_Lock.Acquire();
	adl_panic(m_pSynth);
	m_Lock.Release();

	// Reset MIDI monitor
	CSynthBase::AllSoundOff();
}

void COPLSynth::SetMasterVolume(u8 nVolume)
{
	m_nVolume = nVolume;
	const u8 nConvertedVolume = static_cast<u8>(nVolume / 100.0f * 127.0f);
	const u8 SetMasterVolumeSysEx[] = { 0xF0, 0x7F, 0x7F, 0x04, 0x01, 0x00, nConvertedVolume, 0xF7 };
	m_Lock.Acquire();
	adl_rt_systemExclusive(m_pSynth, SetMasterVolumeSysEx, sizeof(SetMasterVolumeSysEx));
	m_Lock.Release();
}

size_t COPLSynth::Render(float* pOutBuffer, size_t nFrames)
{
	static const ADLMIDI_AudioFormat Format =
	{
		.type = ADLMIDI_SampleType_F32,
		.containerSize = sizeof(float),
		.sampleOffset = 2 * sizeof(float),
	};

	m_Lock.Acquire();
	adl_generateFormat(
		m_pSynth,
		nFrames * 2,
		reinterpret_cast<ADL_UInt8*>(pOutBuffer),
		reinterpret_cast<ADL_UInt8*>(pOutBuffer + 1),
		&Format
	);
	m_Lock.Release();
	return nFrames;
}

size_t COPLSynth::Render(s16* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	adl_generate(m_pSynth, nFrames * 2, pOutBuffer);
	m_Lock.Release();
	return nFrames;
}

void COPLSynth::ReportStatus() const
{
	if (!m_pUI)
		return;

	const char* const* pNames = adl_getBankNames();
	m_pUI->ShowSystemMessage(pNames[m_nCurrentBank]);
}

bool COPLSynth::NextBank()
{
	bool bResult = true;

	m_nCurrentBank = (m_nCurrentBank + 1) % adl_getBanksCount();

	m_Lock.Acquire();
	if (adl_setBank(m_pSynth, m_nCurrentBank) != 0)
		bResult = false;
	else
		adl_reset(m_pSynth);
	m_Lock.Release();

	return bResult;
}
