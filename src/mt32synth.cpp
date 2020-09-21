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

CMT32SynthBase::CMT32SynthBase(FATFS& FileSystem, unsigned pSampleRate, ResamplerQuality pResamplerQuality)
	: mSynth(nullptr),

	  mLowLevel(0),
	  mNullLevel(0),
	  mHighLevel(0),

	  mSampleRate(pSampleRate),
	  mResamplerQuality(pResamplerQuality),
	  mSampleRateConverter(nullptr),

	  mROMManager(FileSystem),
	  mControlROMImage(nullptr),
	  mPCMROMImage(nullptr),

	  mLCD(nullptr)
{
}

CMT32SynthBase::~CMT32SynthBase()
{
	if (mSynth)
		delete mSynth;

	if (mSampleRateConverter)
		delete mSampleRateConverter;
}

bool CMT32SynthBase::Initialize()
{
	if (!mROMManager.ScanROMs())
		return false;

	// Try to load user's preferred initial ROM set, otherwise fall back on first available
	CROMManager::TROMSet initialROMSet = CConfig::Get()->mMT32EmuROMSet;
	if (!mROMManager.HaveROMSet(initialROMSet))
		initialROMSet = CROMManager::TROMSet::Any;

	if (!mROMManager.GetROMSet(initialROMSet, mControlROMImage, mPCMROMImage))
		return false;

	mSynth = new MT32Emu::Synth(this);

	if (!mSynth->open(*mControlROMImage, *mPCMROMImage))
		return false;

	if (mResamplerQuality != ResamplerQuality::None)
	{
		auto quality = MT32Emu::SamplerateConversionQuality_GOOD;
		switch (mResamplerQuality)
		{
			case ResamplerQuality::Fastest:
				quality = MT32Emu::SamplerateConversionQuality_FASTEST;
				break;

			case ResamplerQuality::Fast:
				quality = MT32Emu::SamplerateConversionQuality_FAST;
				break;

			case ResamplerQuality::Good:
				quality = MT32Emu::SamplerateConversionQuality_GOOD;
				break;

			case ResamplerQuality::Best:
				quality = MT32Emu::SamplerateConversionQuality_BEST;
				break;

			default:
				break;
		}

		mSampleRateConverter = new MT32Emu::SampleRateConverter(*mSynth, mSampleRate, quality);
	}

	// Audio output device integer range
	mLowLevel = GetRangeMin();
	mHighLevel = GetRangeMax();
	mNullLevel = (mHighLevel + mLowLevel) / 2;

	return true;
}

void CMT32SynthBase::HandleMIDIShortMessage(u32 pMessage)
{
	// TODO: timestamping
	mSynth->playMsg(pMessage);
}

void CMT32SynthBase::HandleMIDISysExMessage(const u8* pData, size_t pSize)
{
	// TODO: timestamping
	mSynth->playSysex(pData, pSize);
}

void CMT32SynthBase::SetMIDIChannels(MIDIChannels Channels)
{
	if (Channels == MIDIChannels::Standard)
		mSynth->writeSysex(0x10, StandardMIDIChannelsSysEx, sizeof(StandardMIDIChannelsSysEx));
	else
		mSynth->writeSysex(0x10, AlternateMIDIChannelsSysEx, sizeof(AlternateMIDIChannelsSysEx));
}

bool CMT32SynthBase::SwitchROMSet(CROMManager::TROMSet ROMSet)
{
	const MT32Emu::ROMImage* controlROMImage;
	const MT32Emu::ROMImage* pcmROMImage;

	// Get ROM set if available
	if (!mROMManager.GetROMSet(ROMSet, controlROMImage, pcmROMImage))
	{
		if (mLCD)
			mLCD->OnLCDMessage("ROM set not avail!");
		return false;
	}

	// Is this ROM set already active?
	if (controlROMImage == mControlROMImage)
	{
		if (mLCD)
			mLCD->OnLCDMessage("Already selected!");
		return false;
	}

	// Reopen synth with new ROMs
	// N.B. it should be safe to do this without stopping the audio device as render()
	// will just return silence while the synth is closed
	mSynth->close();
	if (!mSynth->open(*controlROMImage, *pcmROMImage))
		return false;

	mControlROMImage = controlROMImage;
	mPCMROMImage     = pcmROMImage;

	if (mLCD)
		mLCD->OnLCDMessage(GetControlROMName());

	return true;
}

const char* CMT32SynthBase::GetControlROMName() const
{
	const char* shortName = mControlROMImage->getROMInfo()->shortName;
	const MT32Emu::Bit8u* romData = mControlROMImage->getFile()->getData();
	size_t offset;

	// Find version strings from ROMs
	if (strstr(shortName, "ctrl_cm32l"))
		offset = 0x2206;
	else if (strstr(shortName, "1_07") || strstr(shortName, "bluer"))
		offset = 0x4011;
	else if (strstr(shortName, "2_04"))
		// FIXME: Find offset from ROM
		return "MT-32 Control v2.04";
	else
		offset = 0x4015;

	return reinterpret_cast<const char*>(romData + offset);
}

u8 CMT32SynthBase::GetVelocityForPart(u8 pPart) const
{
	u8 keys[MT32Emu::DEFAULT_MAX_PARTIALS];
	u8 velocities[MT32Emu::DEFAULT_MAX_PARTIALS];
	u32 playingNotes = mSynth->getPlayingNotes(pPart, keys, velocities);

	u8 maxVelocity = velocities[0];
	for (u32 i = 1; i < playingNotes; ++i)
		if (velocities[i] > maxVelocity)
			maxVelocity = velocities[i];

	return maxVelocity;
}

u8 CMT32SynthBase::GetMasterVolume() const
{
	u8 volume;
	mSynth->readMemory(0x40016, 1, &volume);
	return volume;
}

void CMT32SynthBase::AllSoundOff()
{
	// Stop all sound immediately; MUNT treats CC 0x7C like "All Sound Off", ignoring pedal
	for (uint8_t i = 0; i < 8; ++i)
		mSynth->playMsgOnPart(i, 0xB, 0x7C, 0);
}

bool CMT32SynthBase::onMIDIQueueOverflow()
{
	CLogger::Get()->Write(MT32SynthName, LogError, "MIDI queue overflow");
	return false;
}

void CMT32SynthBase::onProgramChanged(MT32Emu::Bit8u partNum, const char* soundGroupName, const char* patchName)
{
	if (mLCD)
		mLCD->OnProgramChanged(partNum, soundGroupName, patchName);
}

void CMT32SynthBase::printDebug(const char* fmt, va_list list)
{
	//CLogger::Get()->WriteV("debug", LogNotice, fmt, list);
}

void CMT32SynthBase::showLCDMessage(const char* message)
{
	CLogger::Get()->Write(MT32SynthName, LogNotice, "LCD: %s", message);
	if (mLCD)
		mLCD->OnLCDMessage(message);
}

//
// I2S implementation
//
unsigned int CMT32SynthI2S::GetChunk(u32 *pBuffer, unsigned nChunkSize)
{
	float samples[nChunkSize];
	if (mSampleRateConverter)
		mSampleRateConverter->getOutputSamples(samples, nChunkSize / 2);
	else
		mSynth->render(samples, nChunkSize / 2);

	for (size_t i = 0; i < nChunkSize; ++i)
	{
		int level = static_cast<int>(samples[i] * mHighLevel / 2 + mNullLevel);
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
	if (mSampleRateConverter)
		mSampleRateConverter->getOutputSamples(samples, nChunkSize / 2);
	else
		mSynth->render(samples, nChunkSize / 2);

	for (size_t i = 0; i < nChunkSize; ++i)
	{
		int levelLeft = static_cast<int>(samples[i * 2] * mHighLevel / 2 + mNullLevel);
		int levelRight = static_cast<int>(samples[i * 2 + 1] * mHighLevel / 2 + mNullLevel);

		levelLeft = Utility::Clamp(levelLeft, mLowLevel, mHighLevel);
		levelRight = Utility::Clamp(levelRight, mLowLevel, mHighLevel);

		if (mChannelsSwapped)
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
