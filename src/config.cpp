//
// config.cpp
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

#include <cstdlib>

#include <circle/logger.h>
#include <circle/util.h>
#include <fatfs/ff.h>
#include <ini.h>

#include "config.h"
#include "utility.h"

// Templated function that converts a string to an enum
template <class T, const char* pEnumStrings[], size_t N> static bool ParseEnum(const char* pString, T* pOut)
{
	for (size_t i = 0; i < N; ++i)
	{
		if (!strcasecmp(pString, pEnumStrings[i]))
		{
			*pOut = static_cast<T>(i);
			return true;
		}
	}

	return false;
}

// Macro to expand templated enum parser into an overloaded definition of ParseOption()
#define CONFIG_ENUM_PARSER(ENUM_NAME)                                                                           \
	bool CConfig::ParseOption(const char* pString, ENUM_NAME* pOut)                                             \
	{                                                                                                           \
		return ParseEnum<ENUM_NAME, ENUM_NAME##Strings, Utility::ArraySize(ENUM_NAME##Strings)>(pString, pOut); \
	}

const char ConfigName[]    = "config";
const char* TrueStrings[]  = {"true", "on", "1"};
const char* FalseStrings[] = {"false", "off", "0"};

// Enum string tables
CONFIG_ENUM_STRINGS(TSystemDefaultSynth, ENUM_SYSTEMDEFAULTSYNTH);
CONFIG_ENUM_STRINGS(TAudioOutputDevice, ENUM_AUDIOOUTPUTDEVICE);
CONFIG_ENUM_STRINGS(TAudioI2CDACInit, ENUM_AUDIOI2CDACINIT);
CONFIG_ENUM_STRINGS(TMT32EmuResamplerQuality, ENUM_RESAMPLERQUALITY);
CONFIG_ENUM_STRINGS(TMT32EmuMIDIChannels, ENUM_MIDICHANNELS);
CONFIG_ENUM_STRINGS(TMT32EmuROMSet, ENUM_MT32ROMSET);
CONFIG_ENUM_STRINGS(TLCDType, ENUM_LCDTYPE);
CONFIG_ENUM_STRINGS(TControlScheme, ENUM_CONTROLSCHEME);
CONFIG_ENUM_STRINGS(TEncoderType, ENUM_ENCODERTYPE);
CONFIG_ENUM_STRINGS(TLCDRotation, ENUM_LCDROTATION);
CONFIG_ENUM_STRINGS(TNetworkMode, ENUM_NETWORKMODE);

CConfig* CConfig::s_pThis = nullptr;

CConfig::CConfig()
{
	// Expand assignment of all default values from definition file
	#define CFG(_1, _2, MEMBER_NAME, DEFAULT, _3...) MEMBER_NAME = DEFAULT;
	#include "config.def"

	s_pThis = this;
}

bool CConfig::Initialize(const char* pPath)
{
	FIL File;
	if (f_open(&File, pPath, FA_READ) != FR_OK)
	{
		CLogger::Get()->Write(ConfigName, LogError, "Couldn't open '%s' for reading", pPath);
		return false;
	}

	const UINT nSize = f_size(&File);
	char Buffer[nSize];
	UINT nRead;

	if (f_read(&File, Buffer, nSize, &nRead) != FR_OK)
	{
		CLogger::Get()->Write(ConfigName, LogError, "Error reading config file", pPath);
		f_close(&File);
		return false;
	}

	const int nResult = ini_parse_string(Buffer, INIHandler, this);
	if (nResult > 0)
		CLogger::Get()->Write(ConfigName, LogWarning, "Config parse error on line %d", nResult);

	f_close(&File);
	return nResult >= 0;

}

bool CConfig::ParseFXProfileSection(const char* pSection, size_t* pOutFXProfileIndex)
{
	constexpr char SectionPrefix[] = "fluidsynth.soundfont.";
	if (!strstr(pSection, SectionPrefix))
		return false;

	const char* const pProfileName = &pSection[sizeof(SectionPrefix) - 1];
	char* pEnd;

	size_t nIndex = strtoull(pProfileName, &pEnd, 10);
	if (pEnd == pProfileName || *pEnd != '\0' || nIndex >= CSoundFontManager::MaxSoundFonts)
		return false;

	*pOutFXProfileIndex = nIndex;
	return true;
}

bool CConfig::ParseFXProfileOption(const char* pString, const char* pValue, TFXProfile* pOutFXProfile)
{
	#define MATCH(NAME, TYPE, STRUCT_MEMBER)                       \
		if (!strcmp(NAME, pString))                                \
		{                                                          \
			auto Temp = decltype(*pOutFXProfile->STRUCT_MEMBER){}; \
			if (ParseOption(pValue, &Temp))                        \
			{                                                      \
				pOutFXProfile->STRUCT_MEMBER = Temp;               \
				return true;                                       \
			}                                                      \
			return false;                                          \
		}

	MATCH("reverb", bool, bReverbActive);
	MATCH("reverb_damping", float, nReverbDamping);
	MATCH("reverb_level", float, nReverbLevel);
	MATCH("reverb_room_size", float, nReverbRoomSize);
	MATCH("reverb_width", float, nReverbWidth);

	MATCH("chorus", bool, bChorusActive);
	MATCH("chorus_depth", float, nChorusDepth);
	MATCH("chorus_level", float, nChorusLevel);
	MATCH("chorus_voices", int, nChorusVoices);
	MATCH("chorus_speed", float, nChorusSpeed);

	#undef MATCH

	return false;
}

int CConfig::INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue)
{
	CConfig* const pConfig = static_cast<CConfig*>(pUser);
	size_t nFXProfileIndex;

	// Automatically generate ParseOption() calls from macro definition file
	#define BEGIN_SECTION(SECTION)       \
		if (!strcmp(#SECTION, pSection)) \
		{

	#define CFG(INI_NAME, TYPE, MEMBER_NAME, _2, ...) \
		if (!strcmp(#INI_NAME, pName))                \
			return ParseOption(pValue, &pConfig->MEMBER_NAME __VA_OPT__(, ) __VA_ARGS__);

	#define END_SECTION \
		return 0;       \
		}

	#include "config.def"

	// Special handling for SoundFont effects profiles
	if (ParseFXProfileSection(pSection, &nFXProfileIndex))
		return ParseFXProfileOption(pName, pValue, &pConfig->FXProfiles[nFXProfileIndex]);

	return 0;
}

bool CConfig::ParseOption(const char* pString, bool* pOutBool)
{
	for (auto pTrueString : TrueStrings)
	{
		if (!strcasecmp(pString, pTrueString))
		{
			*pOutBool = true;
			return true;
		}
	}

	for (auto pFalseString : FalseStrings)
	{
		if (!strcasecmp(pString, pFalseString))
		{
			*pOutBool = false;
			return true;
		}
	}

	return false;
}

bool CConfig::ParseOption(const char* pString, int* pOutInt, bool bHex)
{
	*pOutInt = strtol(pString, nullptr, bHex ? 16 : 10);
	return true;
}

bool CConfig::ParseOption(const char* pString, float* pOutFloat)
{
	*pOutFloat = strtof(pString, nullptr);
	return true;
}

bool CConfig::ParseOption(const char* pString, CString* pOut)
{
	*pOut = CString(pString);
	return true;
}

bool CConfig::ParseOption(const char* pString, CIPAddress* pOut)
{
	// Space for 4 period-separated groups of 3 digits plus null terminator
	char Buffer[16];
	u8 IPAddress[4];

	strncpy(Buffer, pString, sizeof(Buffer));
	char* pToken = strtok(Buffer, ".");

	for (uint8_t i = 0; i < 4; ++i)
	{
		if (!pToken)
			return false;

		IPAddress[i] = atoi(pToken);
		pToken = strtok(nullptr, ".");
	}

	pOut->Set(IPAddress);
	return true;
}

// Define template function wrappers for parsing enums
CONFIG_ENUM_PARSER(TSystemDefaultSynth);
CONFIG_ENUM_PARSER(TAudioOutputDevice);
CONFIG_ENUM_PARSER(TAudioI2CDACInit);
CONFIG_ENUM_PARSER(TMT32EmuResamplerQuality);
CONFIG_ENUM_PARSER(TMT32EmuMIDIChannels);
CONFIG_ENUM_PARSER(TMT32EmuROMSet);
CONFIG_ENUM_PARSER(TLCDType);
CONFIG_ENUM_PARSER(TControlScheme);
CONFIG_ENUM_PARSER(TEncoderType);
CONFIG_ENUM_PARSER(TLCDRotation);
CONFIG_ENUM_PARSER(TNetworkMode);
