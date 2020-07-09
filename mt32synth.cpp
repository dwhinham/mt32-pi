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

#include <mt32emu/mt32emu.h>

#include "mt32synth.h"

#ifdef BAKE_MT32_ROMS
#include "mt32_control.h"
#include "mt32_pcm.h"
#else
static const char MT32ControlROMName[] = "MT32_CONTROL.ROM";
static const char MT32PCMROMName[] = "MT32_PCM.ROM";
#endif

static const char MT32SynthName[] = "mt32synth";

CMT32SynthBase::CMT32SynthBase(unsigned pSampleRate, ResamplerQuality pResamplerQuality)
	: mSynth(nullptr),

	  mLowLevel(0),
	  mNullLevel(0),
	  mHighLevel(0),

	  mSampleRate(pSampleRate),
	  mResamplerQuality(pResamplerQuality),
	  mSampleRateConverter(nullptr),

	  mLCDMessageHandler(nullptr),

#ifdef BAKE_MT32_ROMS
	  mControlFile(MT32_CONTROL_ROM, MT32_CONTROL_ROM_len),
	  mPCMFile(MT32_PCM_ROM, MT32_PCM_ROM_len),
#endif
	  mControlROMImage(nullptr),
	  mPCMROMImage(nullptr)
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
#ifndef BAKE_MT32_ROMS
	if (!mControlFile.open(MT32ControlROMName))
	{
		CLogger::Get()->Write(MT32SynthName, LogError, "Could not open %s", MT32ControlROMName);
		return false;
	}

	if (!mPCMFile.open(MT32PCMROMName))
	{
		CLogger::Get()->Write(MT32SynthName, LogError, "Could not open %s", MT32PCMROMName);
		return false;
	}
#endif

	mControlROMImage = MT32Emu::ROMImage::makeROMImage(&mControlFile);
	mPCMROMImage = MT32Emu::ROMImage::makeROMImage(&mPCMFile);
	mSynth = new MT32Emu::Synth(this);

	if (mSynth->open(*mControlROMImage, *mPCMROMImage))
	{
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

		return true;
	}

	return false;
}

void CMT32SynthBase::HandleMIDIControlMessage(u32 pMessage)
{
	// TODO: timestamping
	mSynth->playMsg(pMessage);
}

void CMT32SynthBase::HandleMIDISysExMessage(u8* pData, size_t pSize)
{
	// TODO: timestamping
	mSynth->playSysex(pData, pSize);
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

void CMT32SynthBase::printDebug(const char *fmt, va_list list)
{
	//CLogger::Get()->WriteV("debug", LogNotice, fmt, list);
}

void CMT32SynthBase::showLCDMessage(const char *message)
{
	CLogger::Get()->Write("lcd", LogNotice, message);
	if (mLCDMessageHandler)
		mLCDMessageHandler(message);
}

bool CMT32SynthBase::onMIDIQueueOverflow()
{
	CLogger::Get()->Write("midi", LogError, "MIDI queue overflow");
	return false;
}

//
// I2C implementation
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
		int level = static_cast<int>(samples[i] * mHighLevel / 2 + mNullLevel);
		if (level > mHighLevel)
		{
			level = mHighLevel;
		}
		else if (level < 0)
		{
			level = 0;
		}
		*pBuffer++ = static_cast<u32>(level);
	}

	return nChunkSize;
}
