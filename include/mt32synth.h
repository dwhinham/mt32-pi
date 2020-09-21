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
#include <circle/pwmsoundbasedevice.h>
#include <circle/types.h>
#include <fatfs/ff.h>

#include <mt32emu/mt32emu.h>

#include "lcd/mt32lcd.h"
#include "rommanager.h"

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

	enum class MIDIChannels
	{
		Standard,
		Alternate
	};

	CMT32SynthBase(FATFS& FileSystem, unsigned pSampleRate, ResamplerQuality pResamplerQuality);
	virtual ~CMT32SynthBase();

	virtual bool Initialize();

	// CSoundBaseDevice
	virtual bool Start() = 0;
	virtual void Cancel() = 0;

	void HandleMIDIShortMessage(u32 pMessage);
	void HandleMIDISysExMessage(const u8* pData, size_t pSize);
	void SetLCD(CMT32LCD* pLCD) { mLCD = pLCD; }
	void SetMIDIChannels(MIDIChannels Channels);
	bool SwitchROMSet(CROMManager::TROMSet ROMSet);
	const char* GetControlROMName() const;

	u32 GetPartStates() const { return mSynth->getPartStates(); }
	u8 GetVelocityForPart(u8 pPart) const;
	u8 GetMasterVolume() const;

	void AllSoundOff();

protected:
	// CSoundBaseDevice
	virtual int GetRangeMin() const = 0;
	virtual int GetRangeMax() const = 0;

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

	static const u8 StandardMIDIChannelsSysEx[];
	static const u8 AlternateMIDIChannelsSysEx[];

	CROMManager mROMManager;
	const MT32Emu::ROMImage* mControlROMImage;
	const MT32Emu::ROMImage* mPCMROMImage;

	CMT32LCD* mLCD;
};

class CMT32SynthI2S : public CMT32SynthBase, public CI2SSoundBaseDevice
{
public:
	CMT32SynthI2S(FATFS& FileSystem, CInterruptSystem *pInterrupt, unsigned pSampleRate, ResamplerQuality pResamplerQuality, unsigned pChunkSize)
	: CMT32SynthBase(FileSystem, pSampleRate, pResamplerQuality),
	  CI2SSoundBaseDevice(pInterrupt, pSampleRate, pChunkSize)
	{
	}

	virtual int GetRangeMin() const override { return CI2SSoundBaseDevice::GetRangeMin(); }
	virtual int GetRangeMax() const override { return CI2SSoundBaseDevice::GetRangeMax(); }
	virtual bool Start() override { return CI2SSoundBaseDevice::Start(); }
	virtual void Cancel() override { CI2SSoundBaseDevice::Cancel(); }

private:
	// CSoundBaseDevice
	virtual unsigned GetChunk(u32* pBuffer, unsigned nChunkSize) override;
};

class CMT32SynthPWM : public CMT32SynthBase, public CPWMSoundBaseDevice
{
public:
	CMT32SynthPWM(FATFS& FileSystem, CInterruptSystem *pInterrupt, unsigned pSampleRate, ResamplerQuality pResamplerQuality, unsigned pChunkSize)
	: CMT32SynthBase(FileSystem, pSampleRate, pResamplerQuality),
	  CPWMSoundBaseDevice(pInterrupt, pSampleRate, pChunkSize),
	  mChannelsSwapped(AreChannelsSwapped())
	{
	}

	virtual int GetRangeMin() const override { return CPWMSoundBaseDevice::GetRangeMin(); }
	virtual int GetRangeMax() const override { return CPWMSoundBaseDevice::GetRangeMax(); }
	virtual bool Start() override { return CPWMSoundBaseDevice::Start(); }
	virtual void Cancel() override { CPWMSoundBaseDevice::Cancel(); }

private:
	// CSoundBaseDevice
	virtual unsigned GetChunk(u32* pBuffer, unsigned nChunkSize) override;

	bool mChannelsSwapped;
};

#endif
