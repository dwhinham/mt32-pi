//
// mcp23017.cpp
//
// mt32-pi - A bare-metal Roland MT-32 emulator for Raspberry Pi
// Copyright (C) 2020  Dale Whinham <daleyo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <circle/logger.h>

#include "control/mcp23017.h"

#define MCP_IODIRA		0x00
#define MCP_IODIRB		0x01
#define MCP_IPOLA		0x02
#define MCP_IPOLB		0x03
#define MCP_GPINTENA	0x04
#define MCP_GPINTENB	0x05
#define MCP_GPPUA		0x0C
#define MCP_GPPUB		0x0D
#define MCP_GPIOA		0x12
#define MCP_GPIOB		0x13
#define MCP_OLATA		0x14
#define MCP_OLATB		0x15

CMCP23017::CMCP23017(CI2CMaster* pI2CMaster, u8 pAddress)
  : mI2CMaster(pI2CMaster),
	mAddress(pAddress),
	mPortAPrevState(0),
	mPortBPrevState(0)
{
}

bool CMCP23017::Initialize()
{
	assert(mI2CMaster != nullptr);

	// Enable pullup resistors on button inputs
	Write(MCP_GPPUA, 0xFF);
	Write(MCP_GPPUB, 0xFF);

	return true;
}

void CMCP23017::Update()
{
	u8 portAState = Read(MCP_GPIOA);
	u8 portBState = Read(MCP_GPIOB);

	if (mPortAPrevState != portAState)
		CLogger::Get()->Write ("mcp23017", LogNotice, "Port A: 0x%02x", portAState);

	if (mPortBPrevState != portBState)
		CLogger::Get()->Write ("mcp23017", LogNotice, "Port B: 0x%02x", portBState);

	mPortAPrevState = portAState;
	mPortBPrevState = portBState;
}

u8 CMCP23017::Read(u8 pRegister)
{
	u8 value;

	// Set memory pointer to desired register
	mI2CMaster->Write(mAddress, &pRegister, sizeof(pRegister));

	// Get register value
	mI2CMaster->Read(mAddress, &value, sizeof(value));

	return value;
}

void CMCP23017::Write(u8 pRegister, u8 pValue)
{
	u8 buffer[2] = { pRegister, pValue };
	mI2CMaster->Write(mAddress, buffer, sizeof(buffer));
}
