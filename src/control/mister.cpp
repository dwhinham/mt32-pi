//
// mister.cpp
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

#include <circle/logger.h>

#include "control/mister.h"

const char MisterControlName[] = "mistercontrol";

constexpr u8 MisterI2CAddress = 0x45;

CMisterControl::CMisterControl(CI2CMaster* pI2CMaster, TEventQueue& EventQueue)
	: m_pI2CMaster(pI2CMaster),
	  m_pEventQueue(&EventQueue),

	  bMisterActive(false),
	  m_LastSystemStatus{TMisterSynth::Unknown, 0xFF, 0xFF},
	  m_LastMisterStatus{TMisterSynth::Unknown, 0xFF, 0xFF}
{
}

void CMisterControl::Update(const TMisterStatus& SystemStatus)
{
	assert(m_pI2CMaster != nullptr);

	// Read current status from MiSTer
	TMisterStatus MisterStatus;
	if (m_pI2CMaster->Read(MisterI2CAddress, &MisterStatus, sizeof(MisterStatus)) < 0)
	{
		ResetState();
		return;
	}

	//CLogger::Get()->Write(MisterControlName, LogDebug, "MiSTer Rx: 0x%02x 0x%02x 0x%02x", static_cast<u8>(MisterStatus.Synth), MisterStatus.MT32ROMSet, MisterStatus.SoundFontIndex);

	// Core was reset or "Reset Hanging Notes" was selected from OSD; turn off all sound
	if (MisterStatus.Synth == TMisterSynth::Mute)
	{
		CLogger::Get()->Write(MisterControlName, LogNotice, "Stopping synth activity");
		EnqueueAllSoundOffEvent();
		WriteConfigToMister(SystemStatus);
		return;
	}

	if (bMisterActive)
	{
		// If the state has been changed by user controls/SysEx, we just need to update the MiSTer status
		if (SystemStatus != m_LastSystemStatus)
		{
			// Write config back to MiSTer
			if (!WriteConfigToMister(SystemStatus))
			{
				ResetState();
				return;
			}

			m_LastSystemStatus = SystemStatus;
		}
		else if (MisterStatus != m_LastMisterStatus)
		{
			// The state has been changed by MiSTer; apply it
			ApplyConfig(MisterStatus, SystemStatus);

			// Write config back to MiSTer
			if (!WriteConfigToMister(MisterStatus))
			{
				ResetState();
				return;
			}

			m_LastMisterStatus = MisterStatus;
		}
	}
	else
	{
		// First valid reply from MiSTer; apply its config
		ApplyConfig(MisterStatus, SystemStatus);

		// Write config back to MiSTer
		if (!WriteConfigToMister(MisterStatus))
			return;

		// Show MiSTer logo
		EnqueueDisplayImageEvent();

		m_LastMisterStatus = MisterStatus;
		bMisterActive = true;
	}
}

void CMisterControl::ApplyConfig(const TMisterStatus& NewStatus, const TMisterStatus& CurrentStatus)
{
	TEvent Event;

	if (NewStatus.Synth != CurrentStatus.Synth)
	{
		Event.Type = TEventType::SwitchSynth;
		Event.SwitchSynth.Synth = NewStatus.Synth == TMisterSynth::MT32 ? TSynth::MT32 : TSynth::SoundFont;
		m_pEventQueue->Enqueue(Event);
	}

	if (NewStatus.MT32ROMSet != CurrentStatus.MT32ROMSet)
	{
		Event.Type = TEventType::SwitchMT32ROMSet;
		Event.SwitchMT32ROMSet.ROMSet = static_cast<TMT32ROMSet>(NewStatus.MT32ROMSet);
		m_pEventQueue->Enqueue(Event);
	}

	if (NewStatus.SoundFontIndex != CurrentStatus.SoundFontIndex)
	{
		Event.Type = TEventType::SwitchSoundFont;
		Event.SwitchSoundFont.Index = NewStatus.SoundFontIndex;
		m_pEventQueue->Enqueue(Event);
	}
}

bool CMisterControl::WriteConfigToMister(const TMisterStatus& NewStatus)
{
	// Write config back to MiSTer
	if (m_pI2CMaster->Write(MisterI2CAddress, &NewStatus, sizeof(NewStatus)) < 0)
	{
		CLogger::Get()->Write(MisterControlName, LogError, "MiSTer write failed");
		return false;
	}

	//CLogger::Get()->Write(MisterControlName, LogDebug, "MiSTer Tx: 0x%02x 0x%02x 0x%02x", static_cast<u8>(NewStatus.Synth), NewStatus.MT32ROMSet, NewStatus.SoundFontIndex);
	return true;
}

void CMisterControl::ResetState()
{
	if (bMisterActive)
	{
		// MiSTer has just stopped responding; dispatch an All Sound Off event
		CLogger::Get()->Write(MisterControlName, LogNotice, "MiSTer stopped responding; turning notes off");
		EnqueueAllSoundOffEvent();
		bMisterActive = false;
		m_LastSystemStatus = {TMisterSynth::Unknown, 0xFF, 0xFF};
		m_LastMisterStatus = {TMisterSynth::Unknown, 0xFF, 0xFF};
	}
}

void CMisterControl::EnqueueDisplayImageEvent()
{
	TEvent Event;
	Event.Type = TEventType::DisplayImage;
	Event.DisplayImage.Image = TImage::MisterLogo;
	m_pEventQueue->Enqueue(Event);
}

void CMisterControl::EnqueueAllSoundOffEvent()
{
	TEvent Event;
	Event.Type = TEventType::AllSoundOff;
	m_pEventQueue->Enqueue(Event);
}
