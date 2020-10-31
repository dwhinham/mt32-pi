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

#include <mt32emu/mt32emu.h>

#include "lcd/mt32lcd.h"
#include "rommanager.h"
#include "utility.h"

class CMT32SynthBase : public MT32Emu::ReportHandler
{
public:
	#define ENUM_RESAMPLERQUALITY(ENUM) \
		ENUM(None, none)                \
		ENUM(Fastest, fastest)          \
		ENUM(Fast, fast)                \
		ENUM(Good, good)                \
		ENUM(Best, best)

	#define ENUM_MIDICHANNELS(ENUM) \
		ENUM(Standard, standard)    \
		ENUM(Alternate, alternate)

	CONFIG_ENUM(TResamplerQuality, ENUM_RESAMPLERQUALITY);
	CONFIG_ENUM(TMIDIChannels, ENUM_MIDICHANNELS);

	CMT32SynthBase(unsigned nSampleRate, TResamplerQuality ResamplerQuality);
	virtual ~CMT32SynthBase();

	virtual bool Initialize();

	// CSoundBaseDevice
	virtual bool Start() = 0;
	virtual void Cancel() = 0;

	void HandleMIDIShortMessage(u32 nMessage);
	void HandleMIDISysExMessage(const u8* pData, size_t nSize);
	void SetLCD(CMT32LCD* pLCD) { m_pLCD = pLCD; }
	void SetMIDIChannels(TMIDIChannels Channels);
	bool SwitchROMSet(CROMManager::TROMSet ROMSet);
	const char* GetControlROMName() const;

	u32 GetPartStates() const { return m_pSynth->getPartStates(); }
	u8 GetVelocityForPart(u8 nPart) const;
	u8 GetMasterVolume() const;

	void AllSoundOff();

protected:
	// CSoundBaseDevice
	virtual int GetRangeMin() const = 0;
	virtual int GetRangeMax() const = 0;

	MT32Emu::Synth* m_pSynth;

	int m_nLowLevel;
	int m_nNullLevel;
	int m_HighLevel;

	unsigned int m_nSampleRate;
	TResamplerQuality m_ResamplerQuality;
	MT32Emu::SampleRateConverter* m_pSampleRateConverter;

private:
	// ReportHandler
	virtual bool onMIDIQueueOverflow() override;
	virtual void onProgramChanged(MT32Emu::Bit8u nPartNum, const char* pSoundGroupName, const char* pPatchName) override;
	virtual void printDebug(const char* pFmt, va_list pList) override;
	virtual void showLCDMessage(const char* pMessage) override;

	static const u8 StandardMIDIChannelsSysEx[];
	static const u8 AlternateMIDIChannelsSysEx[];

	CROMManager m_ROMManager;
	const MT32Emu::ROMImage* m_pControlROMImage;
	const MT32Emu::ROMImage* m_pPCMROMImage;

	CMT32LCD* m_pLCD;
};

class CMT32SynthI2S : public CMT32SynthBase, public CI2SSoundBaseDevice
{
public:
	CMT32SynthI2S(CInterruptSystem* pInterrupt, unsigned nSampleRate, TResamplerQuality ResamplerQuality, unsigned nChunkSize)
	: CMT32SynthBase(nSampleRate, ResamplerQuality),
	  CI2SSoundBaseDevice(pInterrupt, nSampleRate, nChunkSize)
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
	CMT32SynthPWM(CInterruptSystem* pInterrupt, unsigned nSampleRate, TResamplerQuality ResamplerQuality, unsigned nChunkSize)
	: CMT32SynthBase(nSampleRate, ResamplerQuality),
	  CPWMSoundBaseDevice(pInterrupt, nSampleRate, nChunkSize),
	  m_bChannelsSwapped(AreChannelsSwapped())
	{
	}

	virtual int GetRangeMin() const override { return CPWMSoundBaseDevice::GetRangeMin(); }
	virtual int GetRangeMax() const override { return CPWMSoundBaseDevice::GetRangeMax(); }
	virtual bool Start() override { return CPWMSoundBaseDevice::Start(); }
	virtual void Cancel() override { CPWMSoundBaseDevice::Cancel(); }

private:
	// CSoundBaseDevice
	virtual unsigned GetChunk(u32* pBuffer, unsigned nChunkSize) override;

	bool m_bChannelsSwapped;
};

#endif
