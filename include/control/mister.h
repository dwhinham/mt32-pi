//
// mister.h
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

#ifndef _mister_h
#define _mister_h

#include <circle/i2cmaster.h>
#include <circle/types.h>

#include "event.h"
#include "misterstatus.h"
#include "synth/synth.h"

class CMisterControl
{
public:
	CMisterControl(CI2CMaster* pI2CMaster, TEventQueue& EventQueue);

	void Update(const TMisterStatus& SystemStatus);

private:
	bool WriteConfigToMister(const TMisterStatus& NewStatus);
	void ResetState();
	void ApplyConfig(const TMisterStatus& NewStatus, const TMisterStatus& SystemStatus);
	void EnqueueDisplayImageEvent();
	void EnqueueAllSoundOffEvent();

	CI2CMaster* m_pI2CMaster;
	TEventQueue* m_pEventQueue;

	bool bMisterActive;
	TMisterStatus m_LastSystemStatus;
	TMisterStatus m_LastMisterStatus;
};

#endif
