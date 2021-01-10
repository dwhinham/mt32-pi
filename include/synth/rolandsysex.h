//
// rolandsysex.h
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

#ifndef _rolandsysex_h
#define _rolandsysex_h

#include "synth/sysex.h"
#include "utility.h"

enum class TRolandModelID : u8
{
	GS   = 0x42,
	SC55 = 0x45
};

enum class TRolandCommandID : u8
{
	RQ1 = 0x11,
	DT1 = 0x12,
};

enum class TRolandAddress : u32
{
	SystemModeSet = 0x00007F,

	GSReset          = 0x40007F,
	UseForRhythmPart = 0x401015,

	SC55DisplayText = 0x100000,
	SC55DisplayDots = 0x100100,
};

constexpr u32 DefaultAddressMask   = 0xFFFFFF;
constexpr u32 PatchPartAddressMask = 0xFFF0FF;

template <TRolandModelID TModelID, TRolandAddress TAddress, u32 nAddressMask, size_t nDataSize>
struct TRolandSysExMessage
{
	u8 nStartOfSysEx;
	TManufacturerID ManufacturerID;
	TDeviceID DeviceID;
	TRolandModelID ModelID;
	TRolandCommandID CommandID;
	u8 Address[3];
	u8 Data[nDataSize];
	u8 nChecksum;
	u8 nEndOfExclusive;

	bool IsValid() const
	{
		return	ManufacturerID == TManufacturerID::Roland &&
				ModelID == TModelID &&
				CommandID == TRolandCommandID::DT1 &&
				(GetAddress() & nAddressMask) == static_cast<u32>(TAddress) &&
				nChecksum == Utility::RolandChecksum(Address, sizeof(Address) + nDataSize);
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

using TRolandGSResetSysExMessage          = TRolandSysExMessage<TRolandModelID::GS, TRolandAddress::GSReset, DefaultAddressMask, 1>;
using TRolandSystemModeSetSysExMessage    = TRolandSysExMessage<TRolandModelID::GS, TRolandAddress::SystemModeSet, DefaultAddressMask, 1>;

using TRolandUseForRhythmPartSysExMessage = TRolandSysExMessage<TRolandModelID::GS, TRolandAddress::UseForRhythmPart, PatchPartAddressMask, 1>;

using TSC55DisplayTextSysExMessage = TRolandSysExMessage<TRolandModelID::SC55, TRolandAddress::SC55DisplayText, DefaultAddressMask, 32>;
using TSC55DisplayDotsSysExMessage = TRolandSysExMessage<TRolandModelID::SC55, TRolandAddress::SC55DisplayDots, DefaultAddressMask, 64>;

constexpr size_t RolandSingleDataByteMessageSize = sizeof(TRolandGSResetSysExMessage);

#endif
