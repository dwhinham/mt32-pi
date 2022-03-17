//
// luarepl.cpp
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
#include <circle/net/netsubsystem.h>
#include <circle/sched/scheduler.h>
#include <circle/util.h>

#include "net/luarepl.h"

constexpr u16 LuaREPLPort = 23;

const char LuaREPLName[] = "luarepl";

CLuaREPL::CLuaREPL(lua_State* pState)
	: CTask(TASK_STACK_SIZE, true),
	  m_pLuaState(pState),
	  m_pREPLSocket(nullptr),
	  m_nForeignREPLPort(0),
	  m_REPLBuffer{'\0'},
	  m_nREPLResult(0)
{
}

CLuaREPL::~CLuaREPL()
{
	if (m_pREPLSocket)
		delete m_pREPLSocket;
}

bool CLuaREPL::Initialize()
{
	assert(m_pREPLSocket == nullptr);

	CLogger* const pLogger    = CLogger::Get();
	CNetSubSystem* const pNet = CNetSubSystem::Get();

	if ((m_pREPLSocket = new CSocket(pNet, IPPROTO_TCP)) == nullptr)
		return false;

	if (m_pREPLSocket->Bind(LuaREPLPort) != 0)
	{
		pLogger->Write(LuaREPLName, LogError, "Couldn't bind to port %d", LuaREPLName);
		return false;
	}

	if (m_pREPLSocket->Listen() != 0)
	{
		pLogger->Write(LuaREPLName, LogError, "Couldn't listen on port %d", LuaREPLName);
		return false;
	}

	// We started as a suspended task; run now that initialization is successful
	Start();

	return true;
}

void CLuaREPL::Run()
{
	assert(m_pREPLSocket != nullptr);

	CLogger* const pLogger       = CLogger::Get();
	CScheduler* const pScheduler = CScheduler::Get();

	CSocket* pForeignPeer;
	CIPAddress ForeignIPAddress;
	u16 nForeignPort;

	while (true)
	{
		pForeignPeer = m_pREPLSocket->Accept(&ForeignIPAddress, &nForeignPort);

		size_t nLength = snprintf(m_REPLBuffer, sizeof(m_REPLBuffer), LUA_COPYRIGHT "\n");

		m_nREPLResult = pForeignPeer->Send(m_REPLBuffer, nLength, 0);
		if (m_nREPLResult < 0)
		{
			pLogger->Write(LuaREPLName, LogError, "REPL socket send error: %d", m_nREPLResult);
			delete pForeignPeer;
			continue;
		}

		char LineBuffer[512];
		char* pChar = LineBuffer;

		while (true)
		{
			// Blocking call
			m_nREPLResult = pForeignPeer->Receive(m_REPLBuffer, sizeof(m_REPLBuffer), 0);

			if (m_nREPLResult > 0)
			{
				for (size_t i = 0; i < static_cast<size_t>(m_nREPLResult); ++i)
				{
					if (m_REPLBuffer[i] == '\n' || m_REPLBuffer[i] == '\b')
						continue;

					if (m_REPLBuffer[i] == '\r')
					{
						//*pChar++ = '\r';
						//*pChar++ = '\n';
						*pChar++ = '\0';
						break;
					}

					*pChar++ = m_REPLBuffer[i];
				}

				if (*(pChar - 1) != '\0')
					continue;

				nLength = strlen(LineBuffer);



				if (nLength > 0)
				{
					lua_settop(m_pLuaState, 0);
					//lua_pushlstring(m_pLuaState, LineBuffer, nLength);
					const char* pRetLine = lua_pushfstring(m_pLuaState, "return %s;", LineBuffer);

					int nStatus = luaL_loadbuffer(m_pLuaState, pRetLine, strlen(pRetLine), "=stdin");
					if (nStatus == LUA_OK)
					{
						lua_remove(m_pLuaState, -2);
						// if (LineBuffer[0] != '\0')
						// 	lua_saveline(m_pLuaState, LineBuffer);
					}
					else
						lua_pop(m_pLuaState, 2);  /* pop result from 'luaL_loadbuffer' and modified line */

					lua_remove(m_pLuaState, 1);  /* remove line from the stack */
					lua_assert(lua_gettop(m_pLuaState) == 1);

					// if (nStatus == LUA_OK)
					// {
					// 	nStatus = docall
					// }

					// m_nREPLResult = pForeignPeer->Send(LineBuffer, nLength, 0);
					// if (m_nREPLResult < 0)
					// {
					// 	pLogger->Write(LuaREPLName, LogError, "REPL socket send error: %d", m_nREPLResult);
					// 	delete pForeignPeer;
					// 	break;
					// }
				}

				pChar = LineBuffer;
				*pChar = '\0';
			}
			else
			{
				if (m_nREPLResult < 0)
					pLogger->Write(LuaREPLName, LogError, "REPL socket receive error: %d", m_nREPLResult);
				else
					pLogger->Write(LuaREPLName, LogNotice, "Client disconnected");

				delete pForeignPeer;
				break;
			}

			// Allow other tasks to run
			pScheduler->Yield();
		}

		// Allow other tasks to run
		pScheduler->Yield();
	}
}
