//
// rotaryencoder.h
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

#ifndef _rotaryencoder_h
#define _rotaryencoder_h

#include <circle/gpiopin.h>
#include <circle/types.h>

#include "utility.h"

class CRotaryEncoder
{
public:
	#define ENUM_ENCODERTYPE(ENUM) \
		ENUM(Full, full)           \
		ENUM(Half, half)           \
		ENUM(Quarter, quarter)

	CONFIG_ENUM(TEncoderType, ENUM_ENCODERTYPE);

	CRotaryEncoder(TEncoderType Type, unsigned GPIOPinCLK, unsigned GPIOPinDAT);

	s8 Read();
	void ReadGPIOPins();
	void ReadGPIOPins(bool bCLKValue, bool bDATValue);

private:
	CGPIOPin m_CLKPin;
	CGPIOPin m_DATPin;

	TEncoderType m_Type;
	s8 m_nDelta;
	s8 m_nPreviousState;
};

#endif
