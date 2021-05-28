//
// passthroughsynth.cpp
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

#include <fatfs/ff.h>
#include <circle/logger.h>
#include <circle/timer.h>


#include "config.h"
#include "synth/gmsysex.h"
#include "synth/rolandsysex.h"
#include "synth/passthroughsynth.h"
#include "utility.h"
#include "zoneallocator.h"

CPassthroughSynth::CPassthroughSynth(unsigned nSampleRate, float nGain, u32 nPolyphony)
	: CSynthBase(nSampleRate),

	  m_pSettings(nullptr),
	  m_pSynth(nullptr),

	  m_nInitialGain(nGain),
	  m_nCurrentGain(nGain),

	  m_nPolyphony(nPolyphony),
	  m_nCurrentSoundFontIndex(0)
{
}

CPassthroughSynth::~CPassthroughSynth()
{
}

void CPassthroughSynth::FluidSynthLogCallback(int nLevel, const char* pMessage, void* pUser)
{
}

bool CPassthroughSynth::Initialize()
{
    	return 1;
}

void CPassthroughSynth::HandleMIDIShortMessage(u32 nMessage)
{
}

void CPassthroughSynth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
}

bool CPassthroughSynth::IsActive()
{
    	return 1;
}

void CPassthroughSynth::AllSoundOff()
{
}

void CPassthroughSynth::SetMasterVolume(u8 nVolume)
{
}

size_t CPassthroughSynth::Render(float* pOutBuffer, size_t nFrames)
{
	return nFrames;
}

size_t CPassthroughSynth::Render(s16* pOutBuffer, size_t nFrames)
{
	return nFrames;
}

u8 CPassthroughSynth::GetChannelVelocities(u8* pOutVelocities, size_t nMaxChannels)
{
	return nMaxChannels;
}

void CPassthroughSynth::ReportStatus() const
{
	//if (m_pLCD)
	//	m_pLCD->OnSystemMessage(m_SoundFontManager.GetSoundFontName(m_nCurrentSoundFontIndex));
}

bool CPassthroughSynth::SwitchSoundFont(size_t nIndex)
{
	return true;
}

bool CPassthroughSynth::Reinitialize(const char* pSoundFontPath)
{
	return true;
}
