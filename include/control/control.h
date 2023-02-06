//
// control.h
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2023 Dale Whinham <daleyo@gmail.com>
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

#ifndef _control_h
#define _control_h

#include <circle/gpiopin.h>
#include <circle/types.h>
#include <circle/usertimer.h>

#include "control/rotaryencoder.h"
#include "event.h"
#include "optional.h"
#include "utility.h"

class CControl
{
public:
	CControl(TEventQueue& pEventQueue);
	virtual ~CControl() = default;

	bool Initialize();
	virtual void Update();
	u8 GetButtonState() const { return m_nButtonState; }

protected:
	// Called from interrupt context
	virtual void ReadGPIOPins() = 0;

	void DebounceButtonState(u8 nState, u8 nMask);

	// Must be power of two
	static constexpr size_t ButtonStateHistoryLength = 16;
	static constexpr size_t ButtonStateHistoryMask   = ButtonStateHistoryLength - 1;

	static constexpr u32 RepeatDelayMicros 	   = 500000;	// 500ms
	static constexpr u32 RepeatAccelTimeMicros = 3000000;	// 3s
	static constexpr u32 MaxRepeatPeriodMicros = 100000;	// 10Hz
	static constexpr u32 MinRepeatPeriodMicros = 20000;	// 50Hz

	TEventQueue* m_pEventQueue;
	CUserTimer m_Timer;

	// Debouncing
	u8 m_ButtonStateHistory[ButtonStateHistoryLength];
	size_t m_nButtonStateHistoryIndex;

	// State tracking
	u8 m_nButtonState;
	u8 m_nLastButtonState;

	// Repeat
	TOptional<u8> m_RepeatButton;
	u32 m_PressedTime;
	u32 m_RepeatTime;

	static inline u32 RepeatPeriod(u32 nPressedDuration)
	{
		return Utility::Lerp
		(
			nPressedDuration,
			0,
			RepeatAccelTimeMicros,
			MaxRepeatPeriodMicros,
			MinRepeatPeriodMicros
		);
	}

	static void InterruptHandler(CUserTimer* pUserTimer, void* pParam);
};

class CControlSimpleButtons : public CControl
{
public:
	CControlSimpleButtons(TEventQueue& pEventQueue);

protected:
	virtual void ReadGPIOPins() override;

	CGPIOPin m_GPIOButton1;
	CGPIOPin m_GPIOButton2;
	CGPIOPin m_GPIOButton3;
	CGPIOPin m_GPIOButton4;
};

class CControlSimpleEncoder : public CControl
{
public:
	CControlSimpleEncoder(TEventQueue& pEventQueue, CRotaryEncoder::TEncoderType EncoderType, bool bEncoderReversed);

	virtual void Update() override;

protected:
	virtual void ReadGPIOPins() override;

	CGPIOPin m_GPIOEncoderButton;
	CGPIOPin m_GPIOButton1;
	CGPIOPin m_GPIOButton2;

	CRotaryEncoder m_Encoder;
};

#endif
