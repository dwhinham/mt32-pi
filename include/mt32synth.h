//
// mt32synth.h
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

#ifndef _mt32synth_h
#define _mt32synth_h

#include <circle/i2ssoundbasedevice.h>
#include <circle/interrupt.h>
#include <circle/logger.h>
#include <circle/pwmsoundbasedevice.h>
#include <circle/types.h>

#include <mt32emu/mt32emu.h>

#include "lcd/mt32lcd.h"

#ifdef BAKE_MT32_ROMS
#define MT32_ROM_FILE MT32Emu::ArrayFile
#else
#define MT32_ROM_FILE MT32Emu::FileStream
#endif

class CMT32SynthBase : public MT32Emu::ReportHandler
{
public:
	enum class ResamplerQuality
	{
		None,
		Fastest,
		Fast,
		Good,
		Best
	};

	virtual bool Initialize();

	void HandleMIDIShortMessage(u32 pMessage);
	void HandleMIDISysExMessage(const u8* pData, size_t pSize);
	void SetLCD(CMT32LCD* pLCD) { mLCD = pLCD; }

	u32 GetPartStates() const { return mSynth->getPartStates(); }
	u8 GetVelocityForPart(u8 pPart) const;
	u8 GetMasterVolume() const;

	void AllSoundOff();

protected:
	CMT32SynthBase(unsigned pSampleRate, ResamplerQuality pResamplerQuality);
	virtual ~CMT32SynthBase();

	MT32Emu::Synth *mSynth;

	int mLowLevel;
	int mNullLevel;
	int mHighLevel;

	unsigned int mSampleRate;
	ResamplerQuality mResamplerQuality;
	MT32Emu::SampleRateConverter *mSampleRateConverter;

private:
	// ReportHandler
	virtual bool onMIDIQueueOverflow() override;
	virtual void onProgramChanged(MT32Emu::Bit8u partNum, const char* soundGroupName, const char* patchName) override;
	virtual void printDebug(const char* fmt, va_list list) override;
	virtual void showLCDMessage(const char* message) override;

	CMT32LCD* mLCD;

	MT32_ROM_FILE mControlFile;
	MT32_ROM_FILE mPCMFile;
	const MT32Emu::ROMImage *mControlROMImage;
	const MT32Emu::ROMImage *mPCMROMImage;
};

class CMT32SynthI2S : public CMT32SynthBase, public CI2SSoundBaseDevice
{
public:
	CMT32SynthI2S(CInterruptSystem *pInterrupt, unsigned pSampleRate, ResamplerQuality pResamplerQuality, unsigned pChunkSize)
	: CMT32SynthBase(pSampleRate, pResamplerQuality),
	  CI2SSoundBaseDevice(pInterrupt, pSampleRate, pChunkSize)
	{
		mLowLevel = GetRangeMin();
		mHighLevel = GetRangeMax();
		mNullLevel = (mHighLevel + mLowLevel) / 2;
		CLogger::Get()->Write("mt32synthi2s", LogNotice, "Audio output range: lo=%d null=%d hi=%d", mLowLevel, mNullLevel, mHighLevel);
	}

	virtual bool Initialize()
	{
		if (!CMT32SynthBase::Initialize())
			return false;

		Start();
		return true;
	}

private:
	// CSoundBaseDevice
	virtual unsigned GetChunk(u32 *pBuffer, unsigned nChunkSize);
};

// TODO: Remove duplicate code/decide on better inheritance hierarchy
class CMT32SynthPWM : public CMT32SynthBase, public CPWMSoundBaseDevice
{
public:
	CMT32SynthPWM(CInterruptSystem *pInterrupt, unsigned pSampleRate, ResamplerQuality pResamplerQuality, unsigned pChunkSize)
	: CMT32SynthBase(pSampleRate, pResamplerQuality),
	  CPWMSoundBaseDevice(pInterrupt, pSampleRate, pChunkSize)
	{
		mLowLevel = GetRangeMin();
		mHighLevel = GetRangeMax();
		mNullLevel = (mHighLevel + mLowLevel) / 2;
		CLogger::Get()->Write("mt32synthpwm", LogNotice, "Audio output range: lo=%d null=%d hi=%d", mLowLevel, mNullLevel, mHighLevel);
	}

	virtual bool Initialize()
	{
		if (!CMT32SynthBase::Initialize())
			return false;

		Start();
		return true;
	}

private:
	// CSoundBaseDevice
	virtual unsigned GetChunk(u32 *pBuffer, unsigned nChunkSize);
};

#endif
