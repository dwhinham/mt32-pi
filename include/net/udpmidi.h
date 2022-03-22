//
// udpmidi.h
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

#ifndef _udpmidi_h
#define _udpmidi_h

#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>

class CUDPMIDIHandler
{
public:
	virtual void OnUDPMIDIDataReceived(const u8* pData, size_t nSize) = 0;
};

class CUDPMIDIReceiver : protected CTask
{
public:
	CUDPMIDIReceiver(CUDPMIDIHandler* pHandler);
	virtual ~CUDPMIDIReceiver() override;

	bool Initialize();

	virtual void Run() override;

private:
	// UDP sockets
	CSocket* m_pMIDISocket;

	// Socket receive buffer
	u8 m_MIDIBuffer[FRAME_BUFFER_SIZE];

	// Callback handler
	CUDPMIDIHandler* m_pHandler;
};

#endif
