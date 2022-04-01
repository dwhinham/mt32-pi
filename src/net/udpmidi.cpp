//
// udpmidi.cpp
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

#include <circle/logger.h>
#include <circle/net/in.h>
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>

#include "net/udpmidi.h"

constexpr u16 MIDIPort = 1999;

const char UDPMIDIName[] = "udpmidi";

CUDPMIDIReceiver::CUDPMIDIReceiver(CUDPMIDIHandler* pHandler)
	: CTask(TASK_STACK_SIZE, true),
	  m_pSocket(nullptr),
	  m_ReceiveBuffer{0},
	  m_pHandler(pHandler)
{
}

CUDPMIDIReceiver::~CUDPMIDIReceiver()
{
	if (m_pSocket)
		delete m_pSocket;
}

bool CUDPMIDIReceiver::Initialize()
{
	assert(m_pSocket == nullptr);

	CLogger* const pLogger    = CLogger::Get();
	CNetSubSystem* const pNet = CNetSubSystem::Get();

	if ((m_pSocket = new CSocket(pNet, IPPROTO_UDP)) == nullptr)
		return false;

	if (m_pSocket->Bind(MIDIPort) != 0)
	{
		pLogger->Write(UDPMIDIName, LogError, "Couldn't bind to port %d", MIDIPort);
		return false;
	}

	// We started as a suspended task; run now that initialization is successful
	Start();

	return true;
}

void CUDPMIDIReceiver::Run()
{
	assert(m_pHandler != nullptr);
	assert(m_pSocket != nullptr);

	CLogger* const pLogger       = CLogger::Get();
	CScheduler* const pScheduler = CScheduler::Get();

	while (true)
	{
		// Blocking call
		const int nResult = m_pSocket->Receive(m_ReceiveBuffer, sizeof(m_ReceiveBuffer), 0);

		if (nResult < 0)
			pLogger->Write(UDPMIDIName, LogError, "Socket receive error: %d", nResult);
		else if (nResult > 0)
			m_pHandler->OnUDPMIDIDataReceived(m_ReceiveBuffer, nResult);

		// Allow other tasks to run
		pScheduler->Yield();
	}
}
