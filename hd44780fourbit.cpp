//
// hd44780fourbit.cpp
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

#include <cmath>

#include <circle/logger.h>
#include <circle/sched/scheduler.h>

#include "hd44780.h"

CHD44780FourBit::CHD44780FourBit(u8 pColumns, u8 pRows)
	: CHD44780Base(pColumns, pRows),

	  mRS(GPIO_PIN_RS, GPIOModeOutput),
	  mRW(GPIO_PIN_RW, GPIOModeOutput),
	  mEN(GPIO_PIN_EN, GPIOModeOutput),
	  mD4(GPIO_PIN_D4, GPIOModeOutput),
	  mD5(GPIO_PIN_D5, GPIOModeOutput),
  	  mD6(GPIO_PIN_D6, GPIOModeOutput),
	  mD7(GPIO_PIN_D7, GPIOModeOutput)
{
	// Bring all pins low
	mRS.Write(LOW);
	mRW.Write(LOW);
	mEN.Write(LOW);
	mD4.Write(LOW);
	mD5.Write(LOW);
	mD6.Write(LOW);
	mD7.Write(LOW);
}

void CHD44780FourBit::WriteNybble(u8 pNybble, WriteMode pMode)
{
	// RS = LOW for command mode, HIGH for data mode
	mRS.Write(pMode == WriteMode::Command ? LOW : HIGH);

	mD4.Write((pNybble >> 0) & 1);
	mD5.Write((pNybble >> 1) & 1);
	mD6.Write((pNybble >> 2) & 1);
	mD7.Write((pNybble >> 3) & 1);

	// Toggle enable
	mEN.Write(HIGH);
	mScheduler->usSleep(5);
	mEN.Write(LOW);
	mScheduler->usSleep(100);
}
