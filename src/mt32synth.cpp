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

#include <mt32emu/mt32emu.h>

#include "config.h"
#include "mt32synth.h"
#include "utility.h"

const char MT32SynthName[] = "mt32synth";

// SysEx commands for setting MIDI channel assignment (no SysEx framing, just 3-byte address and 9 channel values)
const u8 CMT32SynthBase::StandardMIDIChannelsSysEx[] = { 0x10, 0x00, 0x0D, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09 };
const u8 CMT32SynthBase::AlternateMIDIChannelsSysEx[] = { 0x10, 0x00, 0x0D, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x09 };

CMT32SynthBase::CMT32SynthBase(FATFS& FileSystem, unsigned nSampleRate, TResamplerQuality ResamplerQuality)
	: m_pSynth(nullptr),

	  m_nLowLevel(0),
	  m_nNullLevel(0),
	  m_HighLevel(0),

	  m_nSampleRate(nSampleRate),
	  m_ResamplerQuality(ResamplerQuality),
	  m_pSampleRateConverter(nullptr),

	  m_ROMManager(FileSystem),
	  m_pControlROMImage(nullptr),
	  m_pPCMROMImage(nullptr),

	  m_pLCD(nullptr)
{
}

CMT32SynthBase::~CMT32SynthBase()
{
	if (m_pSynth)
		delete m_pSynth;

	if (m_pSampleRateConverter)
		delete m_pSampleRateConverter;
}

bool CMT32SynthBase::Initialize()
{
	if (!m_ROMManager.ScanROMs())
		return false;

	// Try to load user's preferred initial ROM set, otherwise fall back on first available
	CROMManager::TROMSet initialROMSet = CConfig::Get()->m_MT32EmuROMSet;
	if (!m_ROMManager.HaveROMSet(initialROMSet))
		initialROMSet = CROMManager::TROMSet::Any;

	if (!m_ROMManager.GetROMSet(initialROMSet, m_pControlROMImage, m_pPCMROMImage))
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

	// Audio output device integer range
	m_nLowLevel = GetRangeMin();
	m_HighLevel = GetRangeMax();
	m_nNullLevel = (m_HighLevel + m_nLowLevel) / 2;

	return true;
}

void CMT32SynthBase::HandleMIDIShortMessage(u32 nMessage)
{
	// TODO: timestamping
	m_pSynth->playMsg(nMessage);
}

void CMT32SynthBase::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	// TODO: timestamping
	m_pSynth->playSysex(pData, nSize);
}

void CMT32SynthBase::SetMIDIChannels(TMIDIChannels Channels)
{
	if (Channels == TMIDIChannels::Standard)
		m_pSynth->writeSysex(0x10, StandardMIDIChannelsSysEx, sizeof(StandardMIDIChannelsSysEx));
	else
		m_pSynth->writeSysex(0x10, AlternateMIDIChannelsSysEx, sizeof(AlternateMIDIChannelsSysEx));
}

bool CMT32SynthBase::SwitchROMSet(CROMManager::TROMSet ROMSet)
{
	const MT32Emu::ROMImage* controlROMImage;
	const MT32Emu::ROMImage* pcmROMImage;

	// Get ROM set if available
	if (!m_ROMManager.GetROMSet(ROMSet, controlROMImage, pcmROMImage))
	{
		if (m_pLCD)
			m_pLCD->OnLCDMessage("ROM set not avail!");
		return false;
	}

	// Is this ROM set already active?
	if (controlROMImage == m_pControlROMImage)
	{
		if (m_pLCD)
			m_pLCD->OnLCDMessage("Already selected!");
		return false;
	}

	// Reopen synth with new ROMs
	// N.B. it should be safe to do this without stopping the audio device as render()
	// will just return silence while the synth is closed
	m_pSynth->close();
	if (!m_pSynth->open(*controlROMImage, *pcmROMImage))
		return false;

	m_pControlROMImage = controlROMImage;
	m_pPCMROMImage     = pcmROMImage;

	if (m_pLCD)
		m_pLCD->OnLCDMessage(GetControlROMName());

	return true;
}

const char* CMT32SynthBase::GetControlROMName() const
{
	// +5 to skip 'ctrl_'
	const char* shortName = m_pControlROMImage->getROMInfo()->shortName + 5;
	const MT32Emu::Bit8u* romData = m_pControlROMImage->getFile()->getData();
	size_t offset;

	// Find version strings from ROMs
	if (strstr(shortName, "cm32l") || strstr(shortName, "2_04"))
		offset = 0x2206;
	else if (strstr(shortName, "1_07") || strstr(shortName, "bluer"))
		offset = 0x4011;
	else
		offset = 0x4015;

	return reinterpret_cast<const char*>(romData + offset);
}

u8 CMT32SynthBase::GetVelocityForPart(u8 nPart) const
{
	u8 keys[MT32Emu::DEFAULT_MAX_PARTIALS];
	u8 velocities[MT32Emu::DEFAULT_MAX_PARTIALS];
	u32 playingNotes = m_pSynth->getPlayingNotes(nPart, keys, velocities);

	u8 maxVelocity = 0;
	for (u32 i = 0; i < playingNotes; ++i)
		if (velocities[i] > maxVelocity)
			maxVelocity = velocities[i];

	return maxVelocity;
}

u8 CMT32SynthBase::GetMasterVolume() const
{
	u8 volume;
	m_pSynth->readMemory(0x40016, 1, &volume);
	return volume;
}

void CMT32SynthBase::AllSoundOff()
{
	// Stop all sound immediately; MUNT treats CC 0x7C like "All Sound Off", ignoring pedal
	for (uint8_t i = 0; i < 8; ++i)
		m_pSynth->playMsgOnPart(i, 0xB, 0x7C, 0);
}

bool CMT32SynthBase::onMIDIQueueOverflow()
{
	CLogger::Get()->Write(MT32SynthName, LogError, "MIDI queue overflow");
	return false;
}

void CMT32SynthBase::onProgramChanged(MT32Emu::Bit8u nPartNum, const char* pSoundGroupName, const char* pPatchName)
{
	if (m_pLCD)
		m_pLCD->OnProgramChanged(nPartNum, pSoundGroupName, pPatchName);
}

void CMT32SynthBase::printDebug(const char* pFmt, va_list pList)
{
	//CLogger::Get()->WriteV("debug", LogNotice, fmt, list);
}

void CMT32SynthBase::showLCDMessage(const char* pMessage)
{
	CLogger::Get()->Write(MT32SynthName, LogNotice, "LCD: %s", pMessage);
	if (m_pLCD)
		m_pLCD->OnLCDMessage(pMessage);
}

//
// I2S implementation
//
unsigned int CMT32SynthI2S::GetChunk(u32 *pBuffer, unsigned nChunkSize)
{
	float samples[nChunkSize];
	if (m_pSampleRateConverter)
		m_pSampleRateConverter->getOutputSamples(samples, nChunkSize / 2);
	else
		m_pSynth->render(samples, nChunkSize / 2);

	for (size_t i = 0; i < nChunkSize; ++i)
	{
		int level = static_cast<int>(samples[i] * m_HighLevel / 2 + m_nNullLevel);
		*pBuffer++ = static_cast<u32>(level);
	}

	return nChunkSize;
}

//
// PWM implementation
//
unsigned int CMT32SynthPWM::GetChunk(u32 *pBuffer, unsigned nChunkSize)
{
	float samples[nChunkSize];
	if (m_pSampleRateConverter)
		m_pSampleRateConverter->getOutputSamples(samples, nChunkSize / 2);
	else
		m_pSynth->render(samples, nChunkSize / 2);

	for (size_t i = 0; i < nChunkSize; ++i)
	{
		int levelLeft = static_cast<int>(samples[i * 2] * m_HighLevel / 2 + m_nNullLevel);
		int levelRight = static_cast<int>(samples[i * 2 + 1] * m_HighLevel / 2 + m_nNullLevel);

		levelLeft = Utility::Clamp(levelLeft, m_nLowLevel, m_HighLevel);
		levelRight = Utility::Clamp(levelRight, m_nLowLevel, m_HighLevel);

		if (m_bChannelsSwapped)
		{
			*pBuffer++ = static_cast<u32>(levelRight);
			*pBuffer++ = static_cast<u32>(levelLeft);
		}
		else
		{
			*pBuffer++ = static_cast<u32>(levelLeft);
			*pBuffer++ = static_cast<u32>(levelRight);
		}
	}

	return nChunkSize;
}
