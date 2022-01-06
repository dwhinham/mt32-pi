//
// control.cpp
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

#include <circle/interrupt.h>

#include "control/control.h"

constexpr u16 PollRateMicros = 1000;

CControl::CControl(TEventQueue& pEventQueue)
	: m_pEventQueue(&pEventQueue),
	  m_Timer(CInterruptSystem::Get(), InterruptHandler, this),

	  m_ButtonStateHistory{0},
	  m_nButtonStateHistoryIndex(0),
	  m_nButtonState(0),
	  m_nLastButtonState(0)
{
}

bool CControl::Initialize()
{
	if (!m_Timer.Initialize())
		return false;

	m_Timer.Start(PollRateMicros);

	return true;
}

void CControl::Update()
{
	TEvent Event;

	if (m_nButtonState != m_nLastButtonState)
	{
		Event.Type = TEventType::Button;

		for (u8 i = 0; i < 8; ++i)
		{
			const bool bCurrentState = m_nButtonState & (1 << i);
			const bool bLastState    = m_nLastButtonState & (1 << i);

			if (bCurrentState != bLastState)
			{
				Event.Button.Button   = static_cast<TButton>(i);
				Event.Button.bPressed = bCurrentState;
				m_pEventQueue->Enqueue(Event);
			}
		}

		m_nLastButtonState = m_nButtonState;
	}
}

void CControl::DebounceButtonState(u8 nState, u8 nMask)
{
	// Use ring buffer for debouncing
	m_ButtonStateHistory[m_nButtonStateHistoryIndex++] = nState;
	m_nButtonStateHistoryIndex &= ButtonStateHistoryMask;

	u8 nDebouncedButtonState = 0xFF;
	for (size_t i = 0; i < ButtonStateHistoryLength; ++i)
		nDebouncedButtonState &= m_ButtonStateHistory[i];

	// Invert so that 1 == "pressed"; mask off button bits
	m_nButtonState = (~nDebouncedButtonState) & nMask;
}

void CControl::InterruptHandler(CUserTimer* pUserTimer, void* pParam)
{
	CControl* const pThis = static_cast<CControl*>(pParam);

	// Re-arm timer
	pUserTimer->Start(PollRateMicros);
	pThis->ReadGPIOPins();
}
