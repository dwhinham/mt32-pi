//
// rotaryencoder.cpp
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

#include "control/rotaryencoder.h"

// Based on encoder reading algorithm by Peter Dannegger: https://embdev.net/articles/Rotary_Encoders

CRotaryEncoder::CRotaryEncoder(TEncoderType Type, unsigned GPIOPinCLK, unsigned GPIOPinDAT)
	: m_CLKPin(GPIOPinCLK, GPIOModeInputPullUp),
	  m_DATPin(GPIOPinDAT, GPIOModeInputPullUp),

	  m_Type(Type),
	  m_nDelta(0),
	  m_nPreviousState(0)
{
	if (m_CLKPin.Read() == LOW)
		m_nPreviousState = 3;

	if (m_DATPin.Read() == LOW)
		m_nPreviousState ^= 1;
}

s8 CRotaryEncoder::Read()
{
	s8 nResult = 0;

	switch (m_Type)
	{
		case TEncoderType::Full:
			nResult = m_nDelta;
			m_nDelta &= 3;
			nResult >>= 2;
			break;

		case TEncoderType::Half:
			nResult = m_nDelta;
			m_nDelta &= 1;
			nResult >>= 1;
			break;

		case TEncoderType::Quarter:
			nResult  = m_nDelta;
			m_nDelta = 0;
			break;
	}

	return nResult;
}

void CRotaryEncoder::ReadGPIOPins()
{
	ReadGPIOPins(m_CLKPin.Read(), m_CLKPin.Read());
}

void CRotaryEncoder::ReadGPIOPins(bool bCLKValue, bool bDATValue)
{
	s8 nNewState = 0;

	// Convert Gray code to binary
	if (bCLKValue == LOW)
		nNewState = 3;

	if (bDATValue == LOW)
		nNewState ^= 1;

	const s8 nDiff = m_nPreviousState - nNewState;

	if (nDiff & 1)
	{
		m_nPreviousState = nNewState;
		m_nDelta += (nDiff & 2) - 1;
	}
}
