//
// ftpdaemon.cpp
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
#include <circle/net/in.h>
#include <circle/net/ipaddress.h>
#include <circle/net/netsubsystem.h>
#include <circle/string.h>

#include "net/ftpdaemon.h"
#include "net/ftpworker.h"

const char FTPDaemonName[] = "ftpd";

constexpr u16 ListenPort = 21;
constexpr u8 MaxConnections = 1;

CFTPDaemon::CFTPDaemon(const char* pUser, const char* pPassword)
	: CTask(TASK_STACK_SIZE, true),
	  m_pListenSocket(nullptr),
	  m_pUser(pUser),
	  m_pPassword(pPassword)
{
}

CFTPDaemon::~CFTPDaemon()
{
	if (m_pListenSocket)
		delete m_pListenSocket;
}

bool CFTPDaemon::Initialize()
{
	CLogger* const pLogger = CLogger::Get();
	CNetSubSystem* const pNet = CNetSubSystem::Get();

	if ((m_pListenSocket = new CSocket(pNet, IPPROTO_TCP)) == nullptr)
		return false;

	if (m_pListenSocket->Bind(ListenPort) != 0)
	{
		pLogger->Write(FTPDaemonName, LogError, "Couldn't bind to port %d", ListenPort);
		return false;
	}

	if (m_pListenSocket->Listen() != 0)
	{
		pLogger->Write(FTPDaemonName, LogError, "Failed to listen on control socket");
		return false;
	}

	// We started as a suspended task; run now that initialization is successful
	Start();

	return true;
}

void CFTPDaemon::Run()
{
	assert(m_pListenSocket != nullptr);

	CLogger* const pLogger = CLogger::Get();
	pLogger->Write(FTPDaemonName, LogNotice, "Listener task spawned");

	while (true)
	{
		CIPAddress ClientIPAddress;
		u16 nClientPort;

		pLogger->Write(FTPDaemonName, LogDebug, "Listener: waiting for connection");
		CSocket* pConnection = m_pListenSocket->Accept(&ClientIPAddress, &nClientPort);

		if (pConnection == nullptr)
		{
			pLogger->Write(FTPDaemonName, LogError, "Unable to accept connection");
			continue;
		}

		CString IPAddressString;
		ClientIPAddress.Format(&IPAddressString);
		pLogger->Write(FTPDaemonName, LogNotice, "Incoming connection from %s:%d", static_cast<const char*>(IPAddressString), nClientPort);

		if (CFTPWorker::GetInstanceCount() >= MaxConnections)
		{
			pConnection->Send("421 Maximum number of connections reached.\r\n", 45, 0);
			delete pConnection;
			pLogger->Write(FTPDaemonName, LogWarning, "Maximum number of connections reached");
			continue;
		}

		// Spawn new worker
		new CFTPWorker(pConnection, m_pUser, m_pPassword);
	}
}
