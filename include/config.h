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

class CConfig
{
public:
	enum class AudioOutputDevice
	{
		PWM,
		I2SDAC
	};

	enum class AudioI2CDACInit
	{
		PCM51xx
	};

	using MT32EmuResamplerQuality = CMT32SynthBase::ResamplerQuality;

	enum class LCDType
	{
		None,
		HD44780FourBit,
		HD44780I2C,
		SSD1306I2C
	};

	CConfig();
	bool Initialize(const char* pPath);

	static CConfig* Get();

	// Expand all config variables from definition file
	#define CFG_A(_1, _2, TYPE, MEMBER_NAME, _4) TYPE MEMBER_NAME;
	#define CFG_S CFG_A
	#include "config.def"

private:
	static int INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue);

	// Overloaded function to parse config options based on their types specified in the definition file
	static bool ParseOption(const char* pString, bool* pOut);
	static bool ParseOption(const char* pString, int* pOut, bool pHex = false);
	static bool ParseOption(const char* pString, AudioOutputDevice* pOut);
	static bool ParseOption(const char* pString, AudioI2CDACInit* pOut);
	static bool ParseOption(const char* pString, MT32EmuResamplerQuality* pOut);
	static bool ParseOption(const char* pString, LCDType* pOut);

	static CConfig* pThis;
};

#endif
