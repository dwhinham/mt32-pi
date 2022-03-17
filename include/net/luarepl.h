//
// luarepl.h
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

#ifndef _luarepl_h
#define _luarepl_h

#include <circle/net/ipaddress.h>
#include <circle/net/socket.h>
#include <circle/sched/task.h>

extern "C"
{
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
}

class CLuaREPL : protected CTask
{
public:
	CLuaREPL(lua_State* pState);
	virtual ~CLuaREPL() override;

	bool Initialize();

	virtual void Run() override;

private:
	lua_State* m_pLuaState;

	// TCP socket
	CSocket* m_pREPLSocket;

	// Foreign peer
	CIPAddress m_ForeignREPLIPAddress;
	u16 m_nForeignREPLPort;

	// Socket receive buffer
	char m_REPLBuffer[FRAME_BUFFER_SIZE];

	int m_nREPLResult;
};

#endif
