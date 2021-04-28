//
// hd44780i2c.cpp
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

#include <circle/timer.h>

#include "lcd/drivers/hd44780.h"

constexpr u8 LCDDataBit      = (1 << 0);
constexpr u8 LCDEnableBit    = (1 << 2);
constexpr u8 LCDBacklightBit = (1 << 3);

CHD44780I2C::CHD44780I2C(CI2CMaster* pI2CMaster, u8 nAddress, u8 nColumns, u8 nRows)
	: CHD44780Base(nColumns, nRows),
	  m_pI2CMaster(pI2CMaster),
	  m_nAddress(nAddress)
{
}

void CHD44780I2C::SetBacklightState(bool bEnabled)
{
	m_bBacklightEnabled = bEnabled;

	// Send a clear command to ensure the backlight bit is updated
	Clear(true);
}

void CHD44780I2C::WriteNybble(u8 nNybble, TWriteMode Mode)
{
	// Write bits with ENABLE pulsed high momentarily
	u8 nByte = ((nNybble << 4) & 0xF0) | LCDEnableBit;

	if (m_bBacklightEnabled)
		nByte |= LCDBacklightBit;

	if (Mode == TWriteMode::Data)
		nByte |= LCDDataBit;

	m_pI2CMaster->Write(m_nAddress, &nByte, 1);
	CTimer::SimpleusDelay(5);

	// Bring ENABLE low again
	nByte &= ~LCDEnableBit;
	m_pI2CMaster->Write(m_nAddress, &nByte, 1);
	CTimer::SimpleusDelay(100);
}
