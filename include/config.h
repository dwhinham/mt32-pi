//
// config.h
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

#ifndef _config_h
#define _config_h

#include <circle/net/ipaddress.h>
#include <circle/types.h>

#include "control/rotaryencoder.h"
#include "lcd/drivers/ssd1306.h"
#include "soundfontmanager.h"
#include "synth/fxprofile.h"
#include "synth/mt32romset.h"
#include "synth/mt32synth.h"
#include "utility.h"

class CConfig
{
public:
	#define ENUM_SYSTEMDEFAULTSYNTH(ENUM) \
		ENUM(MT32, mt32)                  \
		ENUM(SoundFont, soundfont)

	#define ENUM_AUDIOOUTPUTDEVICE(ENUM) \
		ENUM(PWM, pwm)                   \
		ENUM(HDMI, hdmi)                 \
		ENUM(I2S, i2s)

	#define ENUM_AUDIOI2CDACINIT(ENUM) \
		ENUM(None, none)               \
		ENUM(PCM51xx, pcm51xx)

	#define ENUM_CONTROLSCHEME(ENUM)        \
		ENUM(None, none)                    \
		ENUM(SimpleButtons, simple_buttons) \
		ENUM(SimpleEncoder, simple_encoder)

	using TEncoderType             = CRotaryEncoder::TEncoderType;

	using TMT32EmuResamplerQuality = CMT32Synth::TResamplerQuality;
	using TMT32EmuMIDIChannels     = CMT32Synth::TMIDIChannels;
	using TMT32EmuROMSet           = TMT32ROMSet;

	using TLCDRotation             = CSSD1306::TLCDRotation;

	#define ENUM_LCDTYPE(ENUM)             \
		ENUM(None, none)                   \
		ENUM(HD44780FourBit, hd44780_4bit) \
		ENUM(HD44780I2C, hd44780_i2c)      \
		ENUM(SH1106I2C, sh1106_i2c)        \
		ENUM(SSD1306I2C, ssd1306_i2c)

	#define ENUM_NETWORKMODE(ENUM) \
		ENUM(Off, off)             \
		ENUM(Ethernet, ethernet)   \
		ENUM(WiFi, wifi)

	CONFIG_ENUM(TSystemDefaultSynth, ENUM_SYSTEMDEFAULTSYNTH);
	CONFIG_ENUM(TAudioOutputDevice, ENUM_AUDIOOUTPUTDEVICE);
	CONFIG_ENUM(TAudioI2CDACInit, ENUM_AUDIOI2CDACINIT);
	CONFIG_ENUM(TControlScheme, ENUM_CONTROLSCHEME);
	CONFIG_ENUM(TLCDType, ENUM_LCDTYPE);
	CONFIG_ENUM(TNetworkMode, ENUM_NETWORKMODE);

	CConfig();
	bool Initialize(const char* pPath);

	static CConfig* Get() { return s_pThis; }

	// Expand all config variables from definition file
	#define CFG(_1, TYPE, MEMBER_NAME, _2, _3...) TYPE MEMBER_NAME;
	#include "config.def"

	TFXProfile FXProfiles[CSoundFontManager::MaxSoundFonts];

private:
	static int INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue);

	// Overloaded function to parse config options based on their types specified in the definition file
	static bool ParseOption(const char* pString, bool* pOut);
	static bool ParseOption(const char* pString, int* pOut, bool bHex = false);
	static bool ParseOption(const char* pString, float* pOutFloat);
	static bool ParseOption(const char *pString, CString* pOut);
	static bool ParseOption(const char *pString, CIPAddress* pOut);
	static bool ParseOption(const char* pString, TSystemDefaultSynth* pOut);
	static bool ParseOption(const char* pString, TAudioOutputDevice* pOut);
	static bool ParseOption(const char* pString, TAudioI2CDACInit* pOut);
	static bool ParseOption(const char* pString, TMT32EmuResamplerQuality* pOut);
	static bool ParseOption(const char* pString, TMT32EmuMIDIChannels* pOut);
	static bool ParseOption(const char* pString, TMT32EmuROMSet* pOut);
	static bool ParseOption(const char* pString, TLCDType* pOut);
	static bool ParseOption(const char* pString, TControlScheme* pOut);
	static bool ParseOption(const char* pString, TEncoderType* pOut);
	static bool ParseOption(const char* pString, TLCDRotation* pOut);
	static bool ParseOption(const char* pString, TNetworkMode* pOut);

	static bool ParseFXProfileSection(const char* pSection, size_t* pOutFXProfileIndex);
	static bool ParseFXProfileOption(const char* pString, const char* pValue, TFXProfile* pOutFXProfile);

	static CConfig* s_pThis;
};

#endif
