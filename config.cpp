//
// config.cpp
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

#include <cstdlib>
#include <cstring>

#include <circle/logger.h>
#include <ini.h>

#include "config.h"

CConfig* CConfig::pThis = nullptr;

CConfig::CConfig()
{
	// Expand assignment of all default values from definition file
	#define CFG_A(_1, _2, _3, MEMBER_NAME, DEFAULT) MEMBER_NAME = DEFAULT;
	#define CFG_S CFG_A
	#include "config.def"
	
	pThis = this;
}

bool CConfig::Initialize(const char* pPath)
{
	int result = ini_parse(pPath, INIHandler, this);
	if (result == -1)
		CLogger::Get()->Write("config", LogError, "Couldn't open '%s' for reading", pPath);
	else if (result > 0)
		CLogger::Get()->Write("config", LogWarning, "Parse error on line %d", result);

	return result >= 0;
}

CConfig* CConfig::Get()
{
	return pThis;
}

int CConfig::INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue)
{
	auto config = static_cast<CConfig*>(pUser);

	#define MATCH(SECTION, INI_NAME) (!strcmp(SECTION, pSection) && !strcmp(INI_NAME, pName))
	#define CFG_A(SECTION, INI_NAME, _1, MEMBER_NAME, _2) else if (MATCH(#SECTION, #INI_NAME)) { ParseOption(pValue, &config->MEMBER_NAME); }
	#define CFG_S(_1, _2, _3, _4, _5)

	// Handle special cases
	if (MATCH("audio", "i2c_dac_address"))
		ParseOption(pValue, &config->mAudioI2CDACAddress, true);

	// Handle auto cases
	#include "config.def"

	return 1;
} 

bool CConfig::ParseOption(const char* pString, bool* pOutBool)
{
	if (!strcmp(pString, "true") || !strcmp(pString, "on") || !strcmp(pString, "1"))
	{
		*pOutBool = true;
		return true;
	}
	else if (!strcmp(pString, "false") || !strcmp(pString, "off") || !strcmp(pString, "0"))
	{
		*pOutBool = false;
		return true;
	}

	return false;
}

bool CConfig::ParseOption(const char* pString, int* pOutInt, bool pHex)
{
	*pOutInt = strtol(pString, nullptr, pHex ? 16 : 10);
	return true;
}

bool CConfig::ParseOption(const char* pString, AudioOutputDevice* pOut)
{
	if (!strcmp(pString, "pwm"))
	{
		*pOut = AudioOutputDevice::PWM;
		return true;
	}
	else if (!strcmp(pString, "i2s"))
	{
		*pOut = AudioOutputDevice::I2SDAC;
		return true;
	}

	return false;
}

bool CConfig::ParseOption(const char* pString, AudioI2CDACInit* pOut)
{
	if (!strcmp(pString, "pcm51xx"))
	{
		*pOut = AudioI2CDACInit::PCM51xx;
		return true;
	}

	return false;
}

bool CConfig::ParseOption(const char* pString, MT32EmuResamplerQuality* pOut)
{
	if (!strcmp(pString, "none"))
	{
		*pOut = MT32EmuResamplerQuality::None;
		return true;
	}
	else if (!strcmp(pString, "fastest"))
	{
		*pOut = MT32EmuResamplerQuality::Fastest;
		return true;
	}
	else if (!strcmp(pString, "fast"))
	{
		*pOut = MT32EmuResamplerQuality::Fast;
		return true;
	}
	else if (!strcmp(pString, "good"))
	{
		*pOut = MT32EmuResamplerQuality::Good;
		return true;
	}
	else if (!strcmp(pString, "best"))
	{
		*pOut = MT32EmuResamplerQuality::Best;
		return true;
	}

	return false;
}

bool CConfig::ParseOption(const char* pString, LCDType* pOut)
{
	return true;
}
