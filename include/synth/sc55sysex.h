//
// sc55sysex.h
//
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

#ifndef _sc55sysex_h
#define _sc55sysex_h

#include <circle/types.h>

#include "utility.h"

constexpr u8 RolandManufacturerID    = 0x41;
constexpr u8 RolandDeviceIDSC55      = 0x45;
constexpr u8 RolandCommandIDDataSet1 = 0x12;

constexpr u32 SC55DisplayDataAddressText = 0x100000;
constexpr u32 SC55DisplayDataAddressDots = 0x100100;

template <u32 nAddress, size_t nDataSize>
struct TSC55SysExMessage
{
	u8 nStartOfSysEx;
	u8 nManufacturerID;
	u8 nDeviceID;
	u8 nModelID;
	u8 nCommandID;
	u8 Address[3];
	u8 Data[nDataSize];
	u8 nChecksum;
	u8 nEndOfExclusive;

	bool IsValid() const
	{
		return	nStartOfSysEx == 0xF0 &&
				nManufacturerID == RolandManufacturerID &&
				nModelID == RolandDeviceIDSC55 &&
				nCommandID == RolandCommandIDDataSet1 &&
				GetAddress() == nAddress &&
				nChecksum == Utility::RolandChecksum(Address, sizeof(Address) + nDataSize) &&
				nEndOfExclusive == 0xF7;
	}

	u32 GetAddress() const
	{
		return Address[0] << 16 | Address[1] << 8 | Address[2];
	}

	const u8* GetData() const
	{
		return Data;
	}
}
PACKED;

using TSC55DisplayTextSysExMessage = TSC55SysExMessage<SC55DisplayDataAddressText, 32>;
using TSC55DisplayDotsSysExMessage = TSC55SysExMessage<SC55DisplayDataAddressDots, 64>;

#endif
