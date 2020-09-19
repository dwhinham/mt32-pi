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

#include <circle/sched/scheduler.h>

#include "lcd/hd44780.h"

#define LCD_MODE_DATA		0
#define LCD_MODE_COMMAND	1
#define LCD_ENABLE			(1 << 2)
#define LCD_BACKLIGHT		(1 << 3)

CHD44780I2C::CHD44780I2C(CI2CMaster* pI2CMaster, u8 nAddress, u8 nColumns, u8 nRows)
	: CHD44780Base(nColumns, nRows),
	  mI2CMaster(pI2CMaster),
	  mAddress(nAddress)
{
}

void CHD44780I2C::WriteNybble(u8 nNybble, WriteMode Mode)
{
	// Write bits with ENABLE pulsed high momentarily
	u8 bits = ((nNybble << 4) & 0xF0) | LCD_BACKLIGHT | LCD_ENABLE;

	if (Mode == WriteMode::Data)
		bits |= 1;

	mI2CMaster->Write(mAddress, &bits, 1);
	mScheduler->usSleep(5);

	// Bring ENABLE low again
	bits &= ~LCD_ENABLE;
	mI2CMaster->Write(mAddress, &bits, 1);
	mScheduler->usSleep(100);
}
