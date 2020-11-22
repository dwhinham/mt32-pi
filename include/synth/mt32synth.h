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

#include <circle/types.h>

#include <mt32emu/mt32emu.h>

#include "rommanager.h"
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

	CMT32Synth(unsigned nSampleRate, TResamplerQuality ResamplerQuality);
	virtual ~CMT32Synth();

	// CSynthBase
	virtual bool Initialize() override;
	virtual void HandleMIDIShortMessage(u32 nMessage) override;
	virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) override;
	virtual bool IsActive() override { return m_pSynth->isActive(); }
	virtual void AllSoundOff() override;
	virtual size_t Render(s16* pBuffer, size_t nFrames) override;
	virtual size_t Render(float* pBuffer, size_t nFrames) override;
	virtual u8 GetChannelVelocities(u8* pOutVelocities, size_t nMaxChannels) override;

	void SetMIDIChannels(TMIDIChannels Channels);
	bool SwitchROMSet(CROMManager::TROMSet ROMSet);
	const char* GetControlROMName() const;

	u8 GetMasterVolume() const;

private:
	// ReportHandler
	virtual bool onMIDIQueueOverflow() override;
	virtual void onProgramChanged(MT32Emu::Bit8u nPartNum, const char* pSoundGroupName, const char* pPatchName) override;
	virtual void printDebug(const char* pFmt, va_list pList) override;
	virtual void showLCDMessage(const char* pMessage) override;

	static const u8 StandardMIDIChannelsSysEx[];
	static const u8 AlternateMIDIChannelsSysEx[];

	MT32Emu::Synth* m_pSynth;

	TResamplerQuality m_ResamplerQuality;
	MT32Emu::SampleRateConverter* m_pSampleRateConverter;

	CROMManager m_ROMManager;
	const MT32Emu::ROMImage* m_pControlROMImage;
	const MT32Emu::ROMImage* m_pPCMROMImage;
};

#endif
