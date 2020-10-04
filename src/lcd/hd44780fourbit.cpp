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

#include "lcd/hd44780.h"

CHD44780FourBit::CHD44780FourBit(u8 nColumns, u8 nRows)
	: CHD44780Base(nColumns, nRows),

	  m_RS(GPIO_PIN_RS, GPIOModeOutput),
	  m_RW(GPIO_PIN_RW, GPIOModeOutput),
	  m_EN(GPIO_PIN_EN, GPIOModeOutput),
	  m_D4(GPIO_PIN_D4, GPIOModeOutput),
	  m_D5(GPIO_PIN_D5, GPIOModeOutput),
  	  m_D6(GPIO_PIN_D6, GPIOModeOutput),
	  m_D7(GPIO_PIN_D7, GPIOModeOutput)
{
	// Bring all pins low
	m_RS.Write(LOW);
	m_RW.Write(LOW);
	m_EN.Write(LOW);
	m_D4.Write(LOW);
	m_D5.Write(LOW);
	m_D6.Write(LOW);
	m_D7.Write(LOW);
}

void CHD44780FourBit::WriteNybble(u8 nNybble, TWriteMode Mode)
{
	// RS = LOW for command mode, HIGH for data mode
	m_RS.Write(Mode == TWriteMode::Command ? LOW : HIGH);

	m_D4.Write((nNybble >> 0) & 1);
	m_D5.Write((nNybble >> 1) & 1);
	m_D6.Write((nNybble >> 2) & 1);
	m_D7.Write((nNybble >> 3) & 1);

	// Toggle enable
	m_EN.Write(HIGH);
	m_pScheduler->usSleep(5);
	m_EN.Write(LOW);
	m_pScheduler->usSleep(100);
}
