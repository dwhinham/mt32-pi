//
// soundfontsynth.cpp
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

#include <fatfs/ff.h>
#include <circle/logger.h>
#include <circle/timer.h>

#include "config.h"
#include "synth/soundfontsynth.h"
#include "utility.h"

const char SoundFontSynthName[] = "soundfontsynth";
const char SoundFontPath[] = "soundfonts";

extern "C"
{
	// Replacements for fluid_sys.c functions
	FILE* fluid_file_open(const char* path, const char** errMsg)
	{
		FILE* handle = fopen(path, "rb");

		if (!handle && errMsg)
			*errMsg = "Failed to open file";

		return handle;
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

	long default_ftell(void* handle)
	{
		FIL* pFile = static_cast<FIL*>(handle);
		return f_tell(pFile);
	}

	int safe_fread(void* buf, int count, void* fd)
	{
		FIL* pFile = static_cast<FIL*>(fd);
		UINT nRead;
		return f_read(pFile, buf, count, &nRead) == FR_OK ? FLUID_OK : FLUID_FAILED;
	}

	int safe_fseek(void* fd, long ofs, int whence)
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

CSoundFontSynth::CSoundFontSynth(unsigned nSampleRate, u32 nPolyphony)
	: CSynthBase(nSampleRate),

	  m_pSettings(nullptr),
	  m_pSynth(nullptr),

	  m_nPolyphony(nPolyphony),
	  m_nSoundFontID(0),
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
	if (!m_SoundFontManager.ScanSoundFonts())
		return false;

	// Try to get preferred SoundFont
	m_nCurrentSoundFontIndex  = CConfig::Get()->FluidSynthSoundFont;
	const char* soundFontPath = m_SoundFontManager.GetSoundFontPath(m_nCurrentSoundFontIndex);

	// Fall back on first available SoundFont
	if (!soundFontPath)
		soundFontPath = m_SoundFontManager.GetFirstValidSoundFontPath();

	// Give up
	if (!soundFontPath)
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

	fluid_settings_setnum(m_pSettings, "synth.sample-rate", static_cast<double>(m_nSampleRate));
	fluid_settings_setint(m_pSettings, "synth.threadsafe-api", false);

	m_pSynth = new_fluid_synth(m_pSettings);
	if (!m_pSynth)
	{
		CLogger::Get()->Write(SoundFontSynthName, LogError, "Failed to create synth");
		return false;
	}

	m_nSoundFontID = fluid_synth_sfload(m_pSynth, soundFontPath, true);
	if (m_nSoundFontID == FLUID_FAILED)
	{
		CLogger::Get()->Write(SoundFontSynthName, LogError, "Failed to load SoundFont");
		return false;
	}

	fluid_synth_set_polyphony(m_pSynth, m_nPolyphony);

	return true;
}

void CSoundFontSynth::HandleMIDIShortMessage(u32 nMessage)
{
	const u8 status  = nMessage & 0xFF;
	const u8 channel = nMessage & 0xF;
	const u8 data1   = (nMessage >> 8) & 0xFF;
	const u8 data2   = (nMessage >> 16) & 0xFF;

	// Handle system real-time messages
	if (status == 0xFF)
	{
		m_Lock.Acquire();
		fluid_synth_system_reset(m_pSynth);
		m_Lock.Release();
		return;
	}

	m_Lock.Acquire();

	// Handle channel messages
	switch (status & 0xF0)
	{
		// Note off
		case 0x80:
			fluid_synth_noteoff(m_pSynth, channel, data1);
			break;

		// Note on
		case 0x90:
			fluid_synth_noteon(m_pSynth, channel, data1, data2);
			break;

		// Polyphonic key pressure/aftertouch
		case 0xA0:
			fluid_synth_key_pressure(m_pSynth, channel, data1, data2);
			break;

		// Control change
		case 0xB0:
			fluid_synth_cc(m_pSynth, channel, data1, data2);
			break;

		// Program change
		case 0xC0:
			fluid_synth_program_change(m_pSynth, channel, data1);
			break;

		// Channel pressure/aftertouch
		case 0xD0:
			fluid_synth_channel_pressure(m_pSynth, channel, data1);
			break;

		// Pitch bend
		case 0xE0:
			fluid_synth_pitch_bend(m_pSynth, channel, (data2 << 7) | data1);
			break;
	}

	m_Lock.Release();
}

void CSoundFontSynth::HandleMIDISysExMessage(const u8* pData, size_t nSize)
{
	m_Lock.Acquire();
	// Exclude leading 0xF0 and trailing 0xF7
	fluid_synth_sysex(m_pSynth, reinterpret_cast<const char*>(pData + 1), nSize - 1, nullptr, nullptr, nullptr, false);
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

u8 CSoundFontSynth::GetChannelVelocities(u8* pOutVelocities, size_t nMaxChannels)
{
	nMaxChannels = Utility::Min(nMaxChannels, static_cast<size_t>(16));

	m_Lock.Acquire();

	const size_t nVoices = fluid_synth_get_polyphony(m_pSynth);

	// Null-terminated
	fluid_voice_t* Voices[nVoices + 1];
	fluid_voice_t** pCurrentVoice = Voices;

	// Initialize output array
	memset(Voices, 0, (nVoices + 1) * sizeof(*Voices));
	memset(pOutVelocities, 0, nMaxChannels);

	fluid_synth_get_voicelist(m_pSynth, Voices, nVoices, -1);

	while (*pCurrentVoice)
	{
		const u8 nChannel = fluid_voice_get_channel(*pCurrentVoice);
		const u8 nVelocity = fluid_voice_is_on(*pCurrentVoice) ? fluid_voice_get_actual_velocity(*pCurrentVoice) : 0;

		pOutVelocities[nChannel] = Utility::Max(pOutVelocities[nChannel], nVelocity);
		++pCurrentVoice;
	}

	m_Lock.Release();

	return nMaxChannels;
}

bool CSoundFontSynth::SwitchSoundFont(size_t nIndex)
{
	// Is this SoundFont already active?
	if (m_nCurrentSoundFontIndex == nIndex)
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("Already selected!");
		return false;
	}

	// Get SoundFont if available
	const char* soundFontPath = m_SoundFontManager.GetSoundFontPath(nIndex);
	if (!soundFontPath)
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("SoundFont not avail!");
		return false;
	}

	if (m_pLCD)
			m_pLCD->OnSystemMessage("Loading SoundFont...");

	if (m_nSoundFontID >= 0 && fluid_synth_sfunload(m_pSynth, m_nSoundFontID, true) == FLUID_FAILED)
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("SF unload failed!");

		return false;
	}

	// Invalidate SoundFont ID
	m_nSoundFontID = -1;

	int result = fluid_synth_sfload(m_pSynth, soundFontPath, true);
	if (result == FLUID_FAILED)
	{
		if (m_pLCD)
			m_pLCD->OnSystemMessage("SF load failed!");

		return false;
	}

	const char* pName = m_SoundFontManager.GetSoundFontName(nIndex);
	CLogger::Get()->Write(SoundFontSynthName, LogNotice, "Loaded \"%s\"", pName);
	if (m_pLCD)
		m_pLCD->OnSystemMessage(pName);

	m_nCurrentSoundFontIndex = nIndex;
	m_nSoundFontID = result;

	return true;
}

const char* CSoundFontSynth::GetSoundFontName() const
{
	return m_SoundFontManager.GetSoundFontName(m_nCurrentSoundFontIndex);
}
