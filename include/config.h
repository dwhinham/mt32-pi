//
// config.h
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

#ifndef _config_h
#define _config_h

#include "mt32synth.h"
#include "rommanager.h"
#include "utility.h"

class CConfig
{
public:
	#define ENUM_AUDIOOUTPUTDEVICE(ENUM) \
		ENUM(PWM, pwm)                   \
		ENUM(I2SDAC, i2s)

	#define ENUM_AUDIOI2CDACINIT(ENUM) \
		ENUM(None, none)               \
		ENUM(PCM51xx, pcm51xx)

	using TMT32EmuResamplerQuality = CMT32SynthBase::TResamplerQuality;
	using TMT32EmuMIDIChannels     = CMT32SynthBase::TMIDIChannels;
	using TMT32EmuROMSet           = CROMManager::TROMSet;

	#define ENUM_LCDTYPE(ENUM)             \
		ENUM(None, none)                   \
		ENUM(HD44780FourBit, hd44780_4bit) \
		ENUM(HD44780I2C, hd44780_i2c)      \
		ENUM(SSD1306I2C, ssd1306_i2c)

	CONFIG_ENUM(TAudioOutputDevice, ENUM_AUDIOOUTPUTDEVICE);
	CONFIG_ENUM(TAudioI2CDACInit, ENUM_AUDIOI2CDACINIT);
	CONFIG_ENUM(TLCDType, ENUM_LCDTYPE);

	CConfig();
	bool Initialize(const char* pPath);

	static CConfig* Get() { return s_pThis; }

	// Expand all config variables from definition file
	#define CFG(_1, TYPE, MEMBER_NAME, _2, _3...) TYPE MEMBER_NAME;
	#include "config.def"

private:
	static int INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue);

	// Overloaded function to parse config options based on their types specified in the definition file
	static bool ParseOption(const char* pString, bool* pOut);
	static bool ParseOption(const char* pString, int* pOut, bool bHex = false);
	static bool ParseOption(const char* pString, TAudioOutputDevice* pOut);
	static bool ParseOption(const char* pString, TAudioI2CDACInit* pOut);
	static bool ParseOption(const char* pString, TMT32EmuResamplerQuality* pOut);
	static bool ParseOption(const char* pString, TMT32EmuMIDIChannels* pOut);
	static bool ParseOption(const char* pString, TMT32EmuROMSet* pOut);
	static bool ParseOption(const char* pString, TLCDType* pOut);

	static CConfig* s_pThis;
};

#endif
