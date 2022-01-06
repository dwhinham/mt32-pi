//
// simplebuttons.cpp
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

#include "control/button.h"
#include "control/control.h"

constexpr u8 GPIOPinButton1 = 17;
constexpr u8 GPIOPinButton2 = 27;
constexpr u8 GPIOPinButton3 = 22;
constexpr u8 GPIOPinButton4 = 23;

constexpr u8 ButtonMask = 1 << static_cast<u8>(TButton::Button1) |
			  1 << static_cast<u8>(TButton::Button2) |
			  1 << static_cast<u8>(TButton::Button3) |
			  1 << static_cast<u8>(TButton::Button4);

CControlSimpleButtons::CControlSimpleButtons(TEventQueue& pEventQueue)
	: CControl(pEventQueue),

	  m_GPIOButton1(GPIOPinButton1, TGPIOMode::GPIOModeInputPullUp),
	  m_GPIOButton2(GPIOPinButton2, TGPIOMode::GPIOModeInputPullUp),
	  m_GPIOButton3(GPIOPinButton3, TGPIOMode::GPIOModeInputPullUp),
	  m_GPIOButton4(GPIOPinButton4, TGPIOMode::GPIOModeInputPullUp)
{
}

void CControlSimpleButtons::ReadGPIOPins()
{
	// Read current button state from GPIO pins
	const u32 nGPIOState  = CGPIOPin::ReadAll();
	const u8 nButtonState = (((nGPIOState >> GPIOPinButton1) & 1) << static_cast<u8>(TButton::Button1)) |
				(((nGPIOState >> GPIOPinButton2) & 1) << static_cast<u8>(TButton::Button2)) |
				(((nGPIOState >> GPIOPinButton3) & 1) << static_cast<u8>(TButton::Button3)) |
				(((nGPIOState >> GPIOPinButton4) & 1) << static_cast<u8>(TButton::Button4));

	DebounceButtonState(nButtonState, ButtonMask);
}
