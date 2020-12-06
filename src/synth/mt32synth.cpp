//
// mt32synth.cpp
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

#include <circle/logger.h>

#include "config.h"
#include "synth/mt32synth.h"
#include "utility.h"

const char MT32SynthName[] = "mt32synth";

constexpr size_t ROMOffsetVersionStringOld  = 0x4015;
constexpr size_t ROMOffsetVersionString1_07 = 0x4011;
constexpr size_t ROMOffsetVersionStringNew  = 0x2206;
constexpr u32 MemoryAddressMasterVolume     = 0x40016;

// SysEx commands for setting MIDI channel assignment (no SysEx framing, just 3-byte address and 9 channel values)
const u8 CMT32Synth::StandardMIDIChannelsSysEx[] = { 0x10, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
const u8 CMT32Synth::AlternateMIDIChannelsSysEx[] = { 0x10, 0x00, 0x0D, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09 };

CMT32Synth::CMT32Synth(unsigned nSampleRate, TResamplerQuality ResamplerQuality)
	: CSynthBase(nSampleRate),

	  m_pSynth(nullptr),

	  m_ResamplerQuality(ResamplerQuality),
	  m_pSampleRateConverter(nullptr),

	  m_pControlROMImage(nullptr),
	  m_pPCMROMImage(nullptr)
{
}

CMT32Synth::~CMT32Synth()
{
	if (m_pSynth)
		delete m_pSynth;

	if (m_pSampleRateConverter)
		delete m_pSampleRateConverter;
}

bool CMT32Synth::Initialize()
{
	if (!m_ROMManager.ScanROMs())
		return false;

	// Try to load user's preferred initial ROM set, otherwise fall back on first available
	TMT32ROMSet InitialROMSet = CConfig::Get()->MT32EmuROMSet;
	if (!m_ROMManager.HaveROMSet(InitialROMSet))
		InitialROMSet = TMT32ROMSet::Any;

	if (!m_ROMManager.GetROMSet(InitialROMSet, m_pControlROMImage, m_pPCMROMImage))
		return false;

	m_pSynth = new MT32Emu::Synth(this);

	if (!m_pSynth->open(*m_pControlROMImage, *m_pPCMROMImage))
		return false;

	if (m_ResamplerQuality != TResamplerQuality::None)
	{
		auto quality = MT32Emu::SamplerateConversionQuality_GOOD;
		switch (m_ResamplerQuality)
		{
			case TResamplerQuality::Fastest:
				quality = MT32Emu::SamplerateConversionQuality_FASTEST;
				break;

			case TResamplerQuality::Fast:
				quality = MT32Emu::SamplerateConversionQuality_FAST;
				break;

			case TResamplerQuality::Good:
				quality = MT32Emu::SamplerateConversionQuality_GOOD;
				break;

			case TResamplerQuality::Best:
				quality = MT32Emu::SamplerateConversionQuality_BEST;
				break;

			default:
				break;
		}

		m_pSampleRateConverter = new MT32Emu::SampleRateConverter(*m_pSynth, m_nSampleRate, quality);
	}

	return true;
}

void CMT32Synth::HandleMIDIShortMessage(u32 nMessage)
{
	// TODO: timestamping
	m_pSynth->playMsg(nMessage);
}

void CMT32Synth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	// TODO: timestamping
	m_pSynth->playSysex(pData, nSize);
}

void CMT32Synth::AllSoundOff()
{
	// Stop all sound immediately; mt32emu treats CC 0x7C like "All Sound Off", ignoring pedal
	for (uint8_t i = 0; i < 8; ++i)
		m_pSynth->playMsgOnPart(i, 0x0B, 0x7C, 0);
}

size_t CMT32Synth::Render(s16* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	if (m_pSampleRateConverter)
		m_pSampleRateConverter->getOutputSamples(pOutBuffer, nFrames);
	else
		m_pSynth->render(pOutBuffer, nFrames);
	m_Lock.Release();

	return nFrames;
}

size_t CMT32Synth::Render(float* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	if (m_pSampleRateConverter)
		m_pSampleRateConverter->getOutputSamples(pOutBuffer, nFrames);
	else
		m_pSynth->render(pOutBuffer, nFrames);
	m_Lock.Release();

	return nFrames;
}

u8 CMT32Synth::GetChannelVelocities(u8* pOutVelocities, size_t nMaxChannels)
{
	u8 Keys[MT32Emu::DEFAULT_MAX_PARTIALS];
	u8 Velocities[MT32Emu::DEFAULT_MAX_PARTIALS];
	u32 nPartStates = m_pSynth->getPartStates();
	nMaxChannels = Utility::Min(nMaxChannels, static_cast<size_t>(9));

	// Initialize output array
	memset(pOutVelocities, 0, nMaxChannels);

	for (u8 nPart = 0; nPart < nMaxChannels; ++nPart)
	{
		// No active partials on this part; skip
		if (!(nPartStates & 1 << nPart))
			continue;

		// Store maximum velocity for this part
		u32 nPlayingNotes = m_pSynth->getPlayingNotes(nPart, Keys, Velocities);
		for (u8 nNote = 0; nNote < nPlayingNotes; ++nNote)
			pOutVelocities[nPart] = Utility::Max(pOutVelocities[nPart], Velocities[nNote]);
	}

	return nMaxChannels;
}

void CMT32Synth::ReportStatus() const
{
	if (m_pLCD)
		m_pLCD->OnSystemMessage(GetControlROMName());
}

void CMT32Synth::SetMIDIChannels(TMIDIChannels Channels)
{
	if (Channels == TMIDIChannels::Standard)
		m_pSynth->writeSysex(0x10, StandardMIDIChannelsSysEx, sizeof(StandardMIDIChannelsSysEx));
	else
		m_pSynth->writeSysex(0x10, AlternateMIDIChannelsSysEx, sizeof(AlternateMIDIChannelsSysEx));
}

bool CMT32Synth::SwitchROMSet(TMT32ROMSet ROMSet)
{
	const MT32Emu::ROMImage* controlROMImage;
	const MT32Emu::ROMImage* pcmROMImage;

	// Get ROM set if available
	if (!m_ROMManager.GetROMSet(ROMSet, controlROMImage, pcmROMImage))
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("ROM set not avail!");
		return false;
	}

	// Is this ROM set already active?
	if (controlROMImage == m_pControlROMImage)
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("Already selected!");
		return false;
	}

	// Reopen synth with new ROMs
	m_Lock.Acquire();
	m_pSynth->close();
	assert(m_pSynth->open(*controlROMImage, *pcmROMImage));
	m_Lock.Release();

	m_pControlROMImage = controlROMImage;
	m_pPCMROMImage     = pcmROMImage;

	ReportStatus();

	return true;
}

const char* CMT32Synth::GetControlROMName() const
{
	// +5 to skip 'ctrl_'
	const char* pShortName = m_pControlROMImage->getROMInfo()->shortName + 5;
	const MT32Emu::Bit8u* pROMData = m_pControlROMImage->getFile()->getData();
	size_t nOffset;

	// Find version strings from ROMs
	if (strstr(pShortName, "cm32l") || strstr(pShortName, "2_04"))
		nOffset = ROMOffsetVersionStringNew;
	else if (strstr(pShortName, "1_07") || strstr(pShortName, "bluer"))
		nOffset = ROMOffsetVersionString1_07;
	else
		nOffset = ROMOffsetVersionStringOld;

	return reinterpret_cast<const char*>(pROMData + nOffset);
}

u8 CMT32Synth::GetMasterVolume() const
{
	u8 nVolume;
	m_pSynth->readMemory(MemoryAddressMasterVolume, 1, &nVolume);
	return nVolume;
}

bool CMT32Synth::onMIDIQueueOverflow()
{
	CLogger::Get()->Write(MT32SynthName, LogError, "MIDI queue overflow");
	return false;
}

void CMT32Synth::onProgramChanged(MT32Emu::Bit8u nPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	if (m_pLCD)
		m_pLCD->OnProgramChanged(nPartNum, pSoundGroupName, pPatchName);
}

void CMT32Synth::printDebug(const char* pFmt, va_list pList)
{
	//CLogger::Get()->WriteV(MT32SynthName, LogDebug, pFmt, pList);
}

void CMT32Synth::showLCDMessage(const char* pMessage)
{
	CLogger::Get()->Write(MT32SynthName, LogNotice, "LCD: %s", pMessage);
	if (m_pLCD)
		m_pLCD->OnMT32Message(pMessage);
}
