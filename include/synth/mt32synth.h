//
// mt32synth.h
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

#ifndef _mt32synth_h
#define _mt32synth_h

#include <circle/types.h>

#include <mt32emu/mt32emu.h>

#include "rommanager.h"
#include "synth/mt32romset.h"
#include "synth/synthbase.h"
#include "utility.h"

class CMT32Synth : public CSynthBase, public MT32Emu::ReportHandler
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

	CMT32Synth(unsigned nSampleRate, float nGain, float nReverbGain, TResamplerQuality ResamplerQuality);
	virtual ~CMT32Synth();

	// CSynthBase
	virtual bool Initialize() override;
	virtual void HandleMIDIShortMessage(u32 nMessage) override;
	virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) override;
	virtual bool IsActive() override { return m_pSynth->isActive(); }
	virtual void AllSoundOff() override;
	virtual void SetMasterVolume(u8 nVolume) override;
	virtual size_t Render(s16* pBuffer, size_t nFrames) override;
	virtual size_t Render(float* pBuffer, size_t nFrames) override;
	virtual void ReportStatus() const override;
	virtual void UpdateLCD(CLCD& LCD, unsigned int nTicks) override;

	void SetMIDIChannels(TMIDIChannels Channels);
	bool SwitchROMSet(TMT32ROMSet ROMSet);
	bool NextROMSet();
	TMT32ROMSet GetROMSet() const;
	const char* GetControlROMName() const;
	CROMManager& GetROMManager() { return m_ROMManager; }

	u8 GetMasterVolume() const;

private:
	enum class TLCDState
	{
		DisplayingPartStates,
		DisplayingTimbreName,
		DisplayingMessage,
	};

	static constexpr size_t MT32ChannelCount = 9;

	// N characters plus null terminator
	static constexpr size_t LCDTextBufferSize = 20 + 1;
	static constexpr unsigned LCDMessageDisplayTimeMillis = 200;
	static constexpr unsigned LCDTimbreDisplayTimeMillis = 1200;

	void UpdatePartStateText(bool bNarrow);
	void GetPartLevels(unsigned int nTicks, float PartLevels[9], float PartPeaks[9]);

	// MT32Emu::ReportHandler
	virtual bool onMIDIQueueOverflow() override;
	virtual void onProgramChanged(MT32Emu::Bit8u nPartNum, const char* pSoundGroupName, const char* pPatchName) override;
	virtual void printDebug(const char* pFmt, va_list pList) override;
	virtual void showLCDMessage(const char* pMessage) override;
	virtual void onDeviceReset() override;

	static const u8 StandardMIDIChannelsSysEx[];
	static const u8 AlternateMIDIChannelsSysEx[];

	MT32Emu::Synth* m_pSynth;

	float m_nGain;
	float m_nReverbGain;

	TResamplerQuality m_ResamplerQuality;
	MT32Emu::SampleRateConverter* m_pSampleRateConverter;

	CROMManager m_ROMManager;
	TMT32ROMSet m_CurrentROMSet;
	const MT32Emu::ROMImage* m_pControlROMImage;
	const MT32Emu::ROMImage* m_pPCMROMImage;

	// LCD state
	TLCDState m_LCDState;
	unsigned m_nLCDStateTime;
	char m_LCDTextBuffer[LCDTextBufferSize];
	u8 m_nPreviousMasterVolume;
};

#endif
