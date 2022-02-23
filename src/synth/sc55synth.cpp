//
// sc55synth.cpp
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
#include <circle/timer.h>

//#include "config.h"
#include "lcd/ui.h"
#include "synth/sc55synth.h"
#include "utility.h"

#include <string>
#include <vector>

const char SC55SynthName[] = "sc55synth";

const std::string ControlROMPath = "SD:roms/sc55/roland_r15209363.ic23";
const std::vector<std::string> PCMROMPaths =
{
	"SD:/roms/sc55/roland-gss.a_r15209276.ic28",
	"SD:/roms/sc55/roland-gss.b_r15209277.ic27",
	"SD:/roms/sc55/roland-gss.c_r15209281.ic26",
};

CSC55Synth::CSC55Synth(unsigned nSampleRate)
	: CSynthBase(nSampleRate),
	m_ControlROM(ControlROMPath),
	m_PCMROM(PCMROMPaths, m_ControlROM),
	m_Synth(m_ControlROM, m_PCMROM)
{
	m_Synth.set_audio_format(nSampleRate, 2);
}

CSC55Synth::~CSC55Synth()
{
	// if (m_pSynth)
	// 	delete m_pSynth;
}

bool CSC55Synth::Initialize()
{
	return true;
}

void CSC55Synth::HandleMIDIShortMessage(u32 nMessage)
{
	const u8 nStatus  = nMessage & 0xFF;
	const u8 nData1   = (nMessage >> 8) & 0xFF;
	const u8 nData2   = (nMessage >> 16) & 0xFF;
	m_Synth.midi_input(nStatus, nData1, nData2);
}

void CSC55Synth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	// Not implemented in EmuSC yet
}

bool CSC55Synth::IsActive()
{
	m_Lock.Acquire();

	m_Lock.Release();

	return true;
}

void CSC55Synth::AllSoundOff()
{
	// Stop all sound immediately; mt32emu treats CC 0x7C like "All Sound Off", ignoring pedal


	// Reset MIDI monitor
	CSynthBase::AllSoundOff();
}

void CSC55Synth::SetMasterVolume(u8 nVolume)
{
}

size_t CSC55Synth::Render(s16* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	for (size_t i = 0; i < nFrames; ++i)
		m_Synth.get_next_sample(pOutBuffer + i * 2);
	m_Lock.Release();

	return nFrames;
}

size_t CSC55Synth::Render(float* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	s16 IntSamples[2];
	for (size_t i = 0; i < nFrames; ++i)
	{
		m_Synth.get_next_sample(IntSamples);
		pOutBuffer[i * 2] = IntSamples[0] / static_cast<float>(INT16_MAX);
		pOutBuffer[i * 2 + 1] = IntSamples[1] / static_cast<float>(INT16_MAX);
	}
	m_Lock.Release();

	return nFrames;
}

void CSC55Synth::ReportStatus() const
{
}

void CSC55Synth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
}
