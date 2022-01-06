//
// power.h
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

#ifndef _power_h
#define _power_h

#include <circle/bcmpropertytags.h>
#include <circle/types.h>

class CPower
{
public:
	CPower();

	void Update();
	void Awaken();
	void SetPowerSaveTimeout(u16 nSeconds) { m_nPowerSaveTimeout = nSeconds; }

protected:
	virtual void OnEnterPowerSavingMode();
	virtual void OnExitPowerSavingMode();

	virtual void OnThrottleDetected();
	virtual void OnUnderVoltageDetected();

private:
	enum class TState
	{
		Normal,
		PowerSaving
	};

	void UpdateThrottledStatus();

	u16 m_nPowerSaveTimeout;
	unsigned int m_nLastActivityTime;
	TState m_State;

	CBcmPropertyTags m_Tags;
	u32 m_LastThrottledStatus;
};

#endif
