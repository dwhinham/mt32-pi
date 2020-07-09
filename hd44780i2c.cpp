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

#include "hd44780.h"

#define LCD_MODE_DATA		0
#define LCD_MODE_COMMAND	1
#define LCD_ENABLE			(1 << 2)
#define LCD_BACKLIGHT		(1 << 3)

CHD44780I2C::CHD44780I2C(CI2CMaster* pI2CMaster, u8 pAddress, u8 pColumns, u8 pRows)
	: CHD44780Base(pColumns, pRows),
	  mI2CMaster(pI2CMaster),
	  mAddress(pAddress)
{
}

void CHD44780I2C::WriteNybble(u8 pNybble, WriteMode pMode)
{
	// Write bits with ENABLE pulsed high momentarily
	u8 bits = ((pNybble << 4) & 0xF0) | LCD_BACKLIGHT | LCD_ENABLE;

	if (pMode == WriteMode::Data)
		bits |= 1;

	mI2CMaster->Write(mAddress, &bits, 1);
	mScheduler->usSleep(5);

	// Bring ENABLE low again
	bits &= ~LCD_ENABLE;
	mI2CMaster->Write(mAddress, &bits, 1);
	mScheduler->usSleep(100);
}
