//
// hd44780i2c.cpp
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

#include <circle/timer.h>

#include "lcd/hd44780.h"

#define LCD_MODE_DATA		0
#define LCD_MODE_COMMAND	1
#define LCD_ENABLE			(1 << 2)
#define LCD_BACKLIGHT		(1 << 3)

CHD44780I2C::CHD44780I2C(CI2CMaster* pI2CMaster, u8 nAddress, u8 nColumns, u8 nRows)
	: CHD44780Base(nColumns, nRows),
	  m_pI2CMaster(pI2CMaster),
	  m_nAddress(nAddress)
{
}

void CHD44780I2C::SetBacklightEnabled(bool bEnabled)
{
	CSynthLCD::SetBacklightEnabled(bEnabled);

	// Send a clear command to ensure the backlight bit is updated
	Clear(true);
}

void CHD44780I2C::WriteNybble(u8 nNybble, TWriteMode Mode)
{
	// Write bits with ENABLE pulsed high momentarily
	u8 bits = ((nNybble << 4) & 0xF0) | LCD_ENABLE;

	if (m_bBacklightEnabled)
		bits |= LCD_BACKLIGHT;

	if (Mode == TWriteMode::Data)
		bits |= 1;

	m_pI2CMaster->Write(m_nAddress, &bits, 1);
	CTimer::SimpleusDelay(5);

	// Bring ENABLE low again
	bits &= ~LCD_ENABLE;
	m_pI2CMaster->Write(m_nAddress, &bits, 1);
	CTimer::SimpleusDelay(100);
}
