//
// soundfontsynth.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
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

#ifndef _soundfontsynth_h
#define _soundfontsynth_h

#include <circle/types.h>

#include <fluidsynth.h>

#include "soundfontmanager.h"
#include "synth/fxprofile.h"
#include "synth/synthbase.h"

class CSoundFontSynth : public CSynthBase
{
public:
	CSoundFontSynth(unsigned nSampleRate);
	virtual ~CSoundFontSynth() override;

	// CSynthBase
	virtual bool Initialize() override;
	virtual void HandleMIDIShortMessage(u32 nMessage) override;
	virtual void HandleMIDISysExMessage(const u8* pData, size_t nSize) override;
	virtual bool IsActive() override;
	virtual void AllSoundOff() override;
	virtual void SetMasterVolume(u8 nVolume) override;
	virtual size_t Render(s16* pOutBuffer, size_t nFrames) override;
	virtual size_t Render(float* pOutBuffer, size_t nFrames) override;
	virtual void ReportStatus() const override;

	bool SwitchSoundFont(size_t nIndex);
	size_t GetSoundFontIndex() const { return m_nCurrentSoundFontIndex; }
	CSoundFontManager& GetSoundFontManager() { return m_SoundFontManager; }

private:
	bool Reinitialize(const char* pSoundFontPath, const TFXProfile* pFXProfile);
	void ResetMIDIMonitor();
#ifndef NDEBUG
	void DumpFXSettings() const;
#endif
	bool ParseGMSysEx(const u8* pData, size_t nSize);
	bool ParseRolandSysEx(const u8* pData, size_t nSize);
	bool ParseYamahaSysEx(const u8* pData, size_t nSize);

	fluid_settings_t* m_pSettings;
	fluid_synth_t* m_pSynth;

	u8 m_nVolume;
	float m_nInitialGain;

	u16 m_nPercussionMask;
	size_t m_nCurrentSoundFontIndex;

	CSoundFontManager m_SoundFontManager;

	static void FluidSynthLogCallback(int nLevel, const char* pMessage, void* pUser);
};

#endif
