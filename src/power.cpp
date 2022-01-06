//
// power.cpp
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

#include <circle/bcmpropertytags.h>
#include <circle/cputhrottle.h>
#include <circle/logger.h>
#include <circle/timer.h>

#include "power.h"

const char PowerName[] = "power";

// Bits in the throttled status response
constexpr u32 UnderVoltageOccurredBit = 1 << 16;
constexpr u32 ThrottlingOccurredBit   = 1 << 18;

CPower::CPower()
	: m_nPowerSaveTimeout(300),
	  m_nLastActivityTime(0),
	  m_State(TState::Normal),
	  m_LastThrottledStatus(0)
{
}

void CPower::Update()
{
	unsigned int nTicks = CTimer::Get()->GetTicks();

	// Power save timeout
	if (m_State == TState::Normal && m_nPowerSaveTimeout && (nTicks - m_nLastActivityTime) >= m_nPowerSaveTimeout * HZ)
	{
		CCPUThrottle::Get()->SetSpeed(TCPUSpeed::CPUSpeedLow);
		m_State = TState::PowerSaving;
		OnEnterPowerSavingMode();
	}

	// Check for undervoltage and throttling
	UpdateThrottledStatus();
}

void CPower::Awaken()
{
	m_nLastActivityTime = CTimer::Get()->GetTicks();

	if (m_State == TState::Normal)
		return;

	CCPUThrottle::Get()->SetSpeed(TCPUSpeed::CPUSpeedMaximum);
	m_State = TState::Normal;

	OnExitPowerSavingMode();
}

void CPower::OnEnterPowerSavingMode()
{
	CLogger::Get()->Write(PowerName, LogNotice, "Entering power saving mode");
}

void CPower::OnExitPowerSavingMode()
{
	CLogger::Get()->Write(PowerName, LogNotice, "Leaving power saving mode");
}

void CPower::OnThrottleDetected()
{
	CLogger::Get()->Write(PowerName, LogWarning, "CPU throttling by firmware detected; check power supply/cooling");
}

void CPower::OnUnderVoltageDetected()
{
	CLogger::Get()->Write(PowerName, LogWarning, "Undervoltage detected; check power supply");
}

void CPower::UpdateThrottledStatus()
{
	// Get throttled status from the firmware and clear status bits
	TPropertyTagSimple ThrottledStatus;
	ThrottledStatus.nValue = 0xFFFF;
	m_Tags.GetTag(PROPTAG_GET_THROTTLED, &ThrottledStatus, sizeof(ThrottledStatus), sizeof(ThrottledStatus.nValue));

	bool bNewVal = ThrottledStatus.nValue & ThrottlingOccurredBit;
	bool bOldVal = m_LastThrottledStatus & ThrottlingOccurredBit;

	if (bNewVal && bOldVal != bNewVal)
		OnThrottleDetected();

	bNewVal = ThrottledStatus.nValue & UnderVoltageOccurredBit;
	bOldVal = m_LastThrottledStatus & UnderVoltageOccurredBit;

	if (bNewVal && bOldVal != bNewVal)
		OnUnderVoltageDetected();

	m_LastThrottledStatus = ThrottledStatus.nValue;
}
