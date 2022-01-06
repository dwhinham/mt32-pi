//
// hd44780fourbit.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2022 Dale Whinham <daleyo@gmail.com>
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

constexpr u8 GPIOPinRS = 10;
constexpr u8 GPIOPinRW = 9;
constexpr u8 GPIOPinEN = 11;
constexpr u8 GPIOPinD4 = 0;
constexpr u8 GPIOPinD5 = 5;
constexpr u8 GPIOPinD6 = 6;
constexpr u8 GPIOPinD7 = 13;

CHD44780FourBit::CHD44780FourBit(u8 nColumns, u8 nRows)
	: CHD44780Base(nColumns, nRows),

	  m_RS(GPIOPinRS, GPIOModeOutput),
	  m_RW(GPIOPinRW, GPIOModeOutput),
	  m_EN(GPIOPinEN, GPIOModeOutput),
	  m_D4(GPIOPinD4, GPIOModeOutput),
	  m_D5(GPIOPinD5, GPIOModeOutput),
  	  m_D6(GPIOPinD6, GPIOModeOutput),
	  m_D7(GPIOPinD7, GPIOModeOutput)
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
	CTimer::SimpleusDelay(5);
	m_EN.Write(LOW);
	CTimer::SimpleusDelay(100);
}
