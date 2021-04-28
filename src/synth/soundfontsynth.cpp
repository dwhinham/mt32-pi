//
// soundfontsynth.cpp
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

#include <fatfs/ff.h>
#include <circle/logger.h>
#include <circle/timer.h>

#include "config.h"
#include "lcd/ui.h"
#include "synth/gmsysex.h"
#include "synth/rolandsysex.h"
#include "synth/soundfontsynth.h"
#include "utility.h"
#include "zoneallocator.h"

const char SoundFontSynthName[] = "soundfontsynth";
const char SoundFontPath[] = "soundfonts";

extern "C"
{
	// Replacements for fluid_sys.c functions
	void* fluid_alloc(size_t len)
	{
		return CZoneAllocator::Get()->Alloc(len, TZoneTag::FluidSynth);
	}

	void* fluid_realloc(void* ptr, size_t len)
	{
		return CZoneAllocator::Get()->Realloc(ptr, len, TZoneTag::FluidSynth);
	}

	void fluid_free(void* ptr)
	{
		CZoneAllocator::Get()->Free(ptr);
	}

	FILE* fluid_file_open(const char* path, const char** errMsg)
	{
		FILE* pFile = fopen(path, "rb");

		if (!pFile && errMsg)
			*errMsg = "Failed to open file";

		return pFile;
	}

	void fluid_msleep(unsigned int msecs) { CTimer::SimpleMsDelay(msecs); }
	double fluid_utime() { return static_cast<double>(CTimer::GetClockTicks()); }

	// Replacements for fluid_sfont.c functions
	// These were found to be much faster than FluidSynth's default approach of going through libc
	void* default_fopen(const char* path)
	{
		FIL* pFile = new FIL;
		if (f_open(pFile, path, FA_READ) != FR_OK)
		{
			delete pFile;
			pFile = nullptr;
		}

		return pFile;
	}

	int default_fclose(void* handle)
	{
		FIL* pFile = static_cast<FIL*>(handle);

		if (f_close(pFile) == FR_OK)
		{
			delete pFile;
			return FLUID_OK;
		}

		return FLUID_FAILED;
	}

	fluid_long_long_t default_ftell(void* handle)
	{
		FIL* pFile = static_cast<FIL*>(handle);
		return f_tell(pFile);
	}

	int safe_fread(void* buf, fluid_long_long_t count, void* fd)
	{
		FIL* pFile = static_cast<FIL*>(fd);
		UINT nRead;
		return f_read(pFile, buf, count, &nRead) == FR_OK ? FLUID_OK : FLUID_FAILED;
	}

	int safe_fseek(void* fd, fluid_long_long_t ofs, int whence)
	{
		FIL* pFile = static_cast<FIL*>(fd);

		switch (whence)
		{
		case SEEK_CUR:
			ofs += f_tell(pFile);
			break;

		case SEEK_END:
			ofs += f_size(pFile);
			break;

		default:
			break;
		}

		return f_lseek(pFile, ofs) == FR_OK ? FLUID_OK : FLUID_FAILED;
	}
}

CSoundFontSynth::CSoundFontSynth(unsigned nSampleRate, float nGain, u32 nPolyphony)
	: CSynthBase(nSampleRate),

	  m_pSettings(nullptr),
	  m_pSynth(nullptr),

	  m_nInitialGain(nGain),
	  m_nCurrentGain(nGain),

	  m_nPolyphony(nPolyphony),
	  m_nPercussionMask(1 << 9),
	  m_nCurrentSoundFontIndex(0)
{
}

CSoundFontSynth::~CSoundFontSynth()
{
	if (m_pSynth)
		delete_fluid_synth(m_pSynth);

	if (m_pSettings)
		delete_fluid_settings(m_pSettings);
}

void CSoundFontSynth::FluidSynthLogCallback(int nLevel, const char* pMessage, void* pUser)
{
	CLogger::Get()->Write(SoundFontSynthName, static_cast<TLogSeverity>(nLevel), pMessage);
}

bool CSoundFontSynth::Initialize()
{
	const CConfig* const pConfig = CConfig::Get();

	if (!m_SoundFontManager.ScanSoundFonts())
		return false;

	// Try to get preferred SoundFont
	m_nCurrentSoundFontIndex = pConfig->FluidSynthSoundFont;
	const char* pSoundFontPath = m_SoundFontManager.GetSoundFontPath(m_nCurrentSoundFontIndex);

	// Fall back on first available SoundFont
	if (!pSoundFontPath)
	{
		pSoundFontPath = m_SoundFontManager.GetFirstValidSoundFontPath();
		m_nCurrentSoundFontIndex = 0;
	}

	// Give up
	if (!pSoundFontPath)
		return false;

	// Install logging handlers
	fluid_set_log_function(FLUID_PANIC, FluidSynthLogCallback, this);
	fluid_set_log_function(FLUID_ERR, FluidSynthLogCallback, this);
	// fluid_set_log_function(FLUID_WARN, FluidSynthLogCallback, this);
	// fluid_set_log_function(FLUID_INFO, FluidSynthLogCallback, this);
	// fluid_set_log_function(FLUID_DBG, FluidSynthLogCallback, this);

	m_pSettings = new_fluid_settings();
	if (!m_pSettings)
	{
		CLogger::Get()->Write(SoundFontSynthName, LogError, "Failed to create settings");
		return false;
	}

	// Set device ID to match the default Roland Sound Canvas ID so that it recognises some GS SysEx messages
	fluid_settings_setint(m_pSettings, "synth.device-id", static_cast<int>(TDeviceID::SoundCanvasDefault));
	fluid_settings_setnum(m_pSettings, "synth.sample-rate", static_cast<double>(m_nSampleRate));
	fluid_settings_setint(m_pSettings, "synth.threadsafe-api", false);

	return Reinitialize(pSoundFontPath, &pConfig->FXProfiles[m_nCurrentSoundFontIndex]);
}

void CSoundFontSynth::HandleMIDIShortMessage(u32 nMessage)
{
	const u8 nStatus  = nMessage & 0xFF;
	const u8 nChannel = nMessage & 0x0F;
	const u8 nData1   = (nMessage >> 8) & 0xFF;
	const u8 nData2   = (nMessage >> 16) & 0xFF;

	// Handle system real-time messages
	if (nStatus == 0xFF)
	{
		m_Lock.Acquire();
		fluid_synth_system_reset(m_pSynth);
		m_Lock.Release();
		return;
	}

	m_Lock.Acquire();

	// Handle channel messages
	switch (nStatus & 0xF0)
	{
		// Note off
		case 0x80:
			fluid_synth_noteoff(m_pSynth, nChannel, nData1);
			break;

		// Note on
		case 0x90:
			fluid_synth_noteon(m_pSynth, nChannel, nData1, nData2);
			break;

		// Polyphonic key pressure/aftertouch
		case 0xA0:
			fluid_synth_key_pressure(m_pSynth, nChannel, nData1, nData2);
			break;

		// Control change
		case 0xB0:
			fluid_synth_cc(m_pSynth, nChannel, nData1, nData2);
			break;

		// Program change
		case 0xC0:
			fluid_synth_program_change(m_pSynth, nChannel, nData1);
			break;

		// Channel pressure/aftertouch
		case 0xD0:
			fluid_synth_channel_pressure(m_pSynth, nChannel, nData1);
			break;

		// Pitch bend
		case 0xE0:
			fluid_synth_pitch_bend(m_pSynth, nChannel, (nData2 << 7) | nData1);
			break;
	}

	m_Lock.Release();

	// Update MIDI monitor
	CSynthBase::HandleMIDIShortMessage(nMessage);
}

void CSoundFontSynth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	// GM Mode On
	if (nSize == sizeof(TGMModeOnSysExMessage))
	{
		const auto& GMModeOnMessage = reinterpret_cast<const TGMModeOnSysExMessage&>(*pData);
		if (GMModeOnMessage.IsValid())
			ResetMIDIMonitor();
	}

	// Single data byte Roland message
	else if (nSize == RolandSingleDataByteMessageSize)
	{
		const auto& GSResetMessage = reinterpret_cast<const TRolandGSResetSysExMessage&>(*pData);
		const auto& SystemModeSetMessage = reinterpret_cast<const TRolandSystemModeSetSysExMessage&>(*pData);
		const auto& UseForRhythmPartMessage = reinterpret_cast<const TRolandUseForRhythmPartSysExMessage&>(*pData);

		// GS Reset/SC-88 Mode Set		
		if (GSResetMessage.IsValid() || SystemModeSetMessage.IsValid())
			ResetMIDIMonitor();

		// Use For Rhythm Part
		else if (UseForRhythmPartMessage.IsValid())
		{
			// TODO: If FluidSynth had an API to query the channel mode we wouldn't need to keep track of it
			const u8 nChannel = (UseForRhythmPartMessage.GetAddress() >> 8) & 0x0F;
			const u8 nMode    = *UseForRhythmPartMessage.GetData() ? 1 : 0;
			m_nPercussionMask ^= (-nMode ^ m_nPercussionMask) & (1 << nChannel);
		}
	}

	// TODO: XG Mode On reset for MIDI monitor

	// SC-55 display message?
	else if (nSize == sizeof(TSC55DisplayTextSysExMessage))
	{
		const auto& DisplayTextMessage = reinterpret_cast<const TSC55DisplayTextSysExMessage&>(*pData);
		if (DisplayTextMessage.IsValid())
		{
			const char* pMessage = reinterpret_cast<const char*>(DisplayTextMessage.GetData());
			if (m_pUI)
				m_pUI->ShowSC55Text(pMessage);
			return;
		}
	}
	else if (nSize == sizeof(TSC55DisplayDotsSysExMessage))
	{
		const auto& DisplayDotsMessage = reinterpret_cast<const TSC55DisplayDotsSysExMessage&>(*pData);
		if (DisplayDotsMessage.IsValid())
		{
			if (m_pUI)
				m_pUI->ShowSC55Dots(DisplayDotsMessage.GetData());
			return;
		}
	}

	// No special handling; forward to FluidSynth SysEx parser, excluding leading 0xF0 and trailing 0xF7
	m_Lock.Acquire();
	fluid_synth_sysex(m_pSynth, reinterpret_cast<const char*>(pData + 1), nSize - 2, nullptr, nullptr, nullptr, false);
	m_Lock.Release();
}

bool CSoundFontSynth::IsActive()
{
	m_Lock.Acquire();
	int nVoices = fluid_synth_get_active_voice_count(m_pSynth);
	m_Lock.Release();

	return nVoices > 0;
}

void CSoundFontSynth::AllSoundOff()
{
	m_Lock.Acquire();
	fluid_synth_all_sounds_off(m_pSynth, -1);
	m_Lock.Release();

	// Reset MIDI monitor
	CSynthBase::AllSoundOff();
}

void CSoundFontSynth::SetMasterVolume(u8 nVolume)
{
	m_nCurrentGain = nVolume / 100.0f * m_nInitialGain;
	m_Lock.Acquire();
	fluid_synth_set_gain(m_pSynth, m_nCurrentGain);
	m_Lock.Release();
}

size_t CSoundFontSynth::Render(float* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	assert(fluid_synth_write_float(m_pSynth, nFrames, pOutBuffer, 0, 2, pOutBuffer, 1, 2) == FLUID_OK);
	m_Lock.Release();
	return nFrames;
}

size_t CSoundFontSynth::Render(s16* pOutBuffer, size_t nFrames)
{
	m_Lock.Acquire();
	assert(fluid_synth_write_s16(m_pSynth, nFrames, pOutBuffer, 0, 2, pOutBuffer, 1, 2) == FLUID_OK);
	m_Lock.Release();
	return nFrames;
}

void CSoundFontSynth::ReportStatus() const
{
	if (m_pUI)
		m_pUI->ShowSystemMessage(m_SoundFontManager.GetSoundFontName(m_nCurrentSoundFontIndex));
}

void CSoundFontSynth::UpdateLCD(CLCD& LCD, unsigned int nTicks)
{
	const u8 nBarHeight = LCD.Height();
	float ChannelLevels[16], PeakLevels[16];
	m_MIDIMonitor.GetChannelLevels(nTicks, ChannelLevels, PeakLevels, m_nPercussionMask);
	CUserInterface::DrawChannelLevels(LCD, nBarHeight, ChannelLevels, PeakLevels, 16, true);
}

bool CSoundFontSynth::SwitchSoundFont(size_t nIndex)
{
	// Is this SoundFont already active?
	if (m_nCurrentSoundFontIndex == nIndex)
	{
		if (m_pUI)
			m_pUI->ShowSystemMessage("Already selected!");
		return false;
	}

	// Get SoundFont if available
	const char* pSoundFontPath = m_SoundFontManager.GetSoundFontPath(nIndex);
	if (!pSoundFontPath)
	{
		if (m_pUI)
			m_pUI->ShowSystemMessage("SoundFont not avail!");
		return false;
	}

	if (m_pUI)
		m_pUI->ShowSystemMessage("Loading SoundFont", true);

	// We can't use fluid_synth_sfunload() as we don't support the lazy SoundFont unload timer, so trash the entire synth and create a new one
	if (!Reinitialize(pSoundFontPath, &CConfig::Get()->FXProfiles[nIndex]))
	{
		if (m_pUI)
			m_pUI->ShowSystemMessage("SF switch failed!");

		return false;
	}

	m_nCurrentSoundFontIndex = nIndex;

	CLogger::Get()->Write(SoundFontSynthName, LogNotice, "Loaded \"%s\"", m_SoundFontManager.GetSoundFontName(nIndex));
	if (m_pUI)
		m_pUI->ClearSpinnerMessage();

	return true;
}

bool CSoundFontSynth::Reinitialize(const char* pSoundFontPath, const TFXProfile* pFXProfile)
{
	const CConfig* const pConfig = CConfig::Get();
	CLogger* const pLogger = CLogger::Get();

	m_Lock.Acquire();

	if (m_pSynth)
		delete_fluid_synth(m_pSynth);

	m_pSynth = new_fluid_synth(m_pSettings);

	if (!m_pSynth)
	{
		m_Lock.Release();
		pLogger->Write(SoundFontSynthName, LogError, "Failed to create synth");
		return false;
	}

	fluid_synth_set_gain(m_pSynth, m_nCurrentGain);
	fluid_synth_set_polyphony(m_pSynth, m_nPolyphony);

	// Use values from effects profile if set, otherwise use defaults
	fluid_synth_reverb_on(m_pSynth, -1, pFXProfile->bReverbActive.ValueOr(pConfig->FluidSynthDefaultReverbActive));
	fluid_synth_set_reverb_group_damp(m_pSynth, -1, pFXProfile->nReverbDamping.ValueOr(pConfig->FluidSynthDefaultReverbDamping));
	fluid_synth_set_reverb_group_level(m_pSynth, -1, pFXProfile->nReverbLevel.ValueOr(pConfig->FluidSynthDefaultReverbLevel));
	fluid_synth_set_reverb_group_roomsize(m_pSynth, -1, pFXProfile->nReverbRoomSize.ValueOr(pConfig->FluidSynthDefaultReverbRoomSize));
	fluid_synth_set_reverb_group_width(m_pSynth, -1, pFXProfile->nReverbWidth.ValueOr(pConfig->FluidSynthDefaultReverbWidth));

	fluid_synth_chorus_on(m_pSynth, -1,	pFXProfile->bChorusActive.ValueOr(pConfig->FluidSynthDefaultChorusActive));
	fluid_synth_set_chorus_group_depth(m_pSynth, -1, pFXProfile->nChorusDepth.ValueOr(pConfig->FluidSynthDefaultChorusDepth));
	fluid_synth_set_chorus_group_level(m_pSynth, -1, pFXProfile->nChorusLevel.ValueOr(pConfig->FluidSynthDefaultChorusLevel));
	fluid_synth_set_chorus_group_nr(m_pSynth, -1, pFXProfile->nChorusVoices.ValueOr(pConfig->FluidSynthDefaultChorusVoices));
	fluid_synth_set_chorus_group_speed(m_pSynth, -1, pFXProfile->nChorusSpeed.ValueOr(pConfig->FluidSynthDefaultChorusSpeed));

#ifndef NDEBUG
	DumpFXSettings();
#endif

	ResetMIDIMonitor();

	m_Lock.Release();

	const unsigned int nLoadStart = CTimer::GetClockTicks();

	if (fluid_synth_sfload(m_pSynth, pSoundFontPath, true) == FLUID_FAILED)
	{
		pLogger->Write(SoundFontSynthName, LogError, "Failed to load SoundFont");
		return false;
	}

	const float nLoadTime = (CTimer::GetClockTicks() - nLoadStart) / 1000000.0f;
	pLogger->Write(SoundFontSynthName, TLogSeverity::LogNotice, "\"%s\" loaded in %0.2f seconds", pSoundFontPath, nLoadTime);

	return true;
}

void CSoundFontSynth::ResetMIDIMonitor()
{
	m_MIDIMonitor.AllNotesOff();
	m_MIDIMonitor.ResetControllers(false);
	m_nPercussionMask = 1 << 9;
}

#ifndef NDEBUG
void CSoundFontSynth::DumpFXSettings() const
{
	CLogger* const pLogger = CLogger::Get();
	double nReverbDamping, nReverbLevel, nReverbRoomSize, nReverbWidth, nChorusDepth, nChorusLevel, nChorusSpeed;
	int nChorusVoices;

	assert(fluid_synth_get_reverb_group_damp(m_pSynth, -1, &nReverbDamping) == FLUID_OK);
	assert(fluid_synth_get_reverb_group_level(m_pSynth, -1, &nReverbLevel) == FLUID_OK);
	assert(fluid_synth_get_reverb_group_roomsize(m_pSynth, -1, &nReverbRoomSize) == FLUID_OK);
	assert(fluid_synth_get_reverb_group_width(m_pSynth, -1, &nReverbWidth) == FLUID_OK);

	assert(fluid_synth_get_chorus_group_depth(m_pSynth, -1, &nChorusDepth) == FLUID_OK);
	assert(fluid_synth_get_chorus_group_level(m_pSynth, -1, &nChorusLevel) == FLUID_OK);
	assert(fluid_synth_get_chorus_group_nr(m_pSynth, -1, &nChorusVoices) == FLUID_OK);
	assert(fluid_synth_get_chorus_group_speed(m_pSynth, -1, &nChorusSpeed) == FLUID_OK);

	pLogger->Write(SoundFontSynthName, LogNotice,
		"Reverb: %.2f, %.2f, %.2f, %.2f",
		nReverbDamping,
		nReverbLevel,
		nReverbRoomSize,
		nReverbWidth
	);

	pLogger->Write(SoundFontSynthName, LogNotice,
		"Chorus: %.2f, %.2f, %d, %.2f",
		nChorusDepth,
		nChorusLevel,
		nChorusVoices,
		nChorusSpeed
	);
}
#endif
