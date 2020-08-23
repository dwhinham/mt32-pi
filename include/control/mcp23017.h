//
// mcp23017.h
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

#ifndef _mcp23017_h
#define _mcp23017_h

#include <circle/i2cmaster.h>
#include <circle/types.h>

class CMCP23017
{
public:
	CMCP23017(CI2CMaster* pI2CMaster, u8 pAddress = 0x20);

	bool Initialize();
	void Update();

private:
	u8 Read(u8 pRegister);
	void Write(u8 pRegister, u8 pValue);

	CI2CMaster* mI2CMaster;
	u8 mAddress;

	u8 mPortAPrevState;
	u8 mPortBPrevState;
};

#endif
