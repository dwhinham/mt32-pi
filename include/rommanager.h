//
// rommanager.h
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

#ifndef _rommanager_h
#define _rommanager_h

#include <mt32emu/mt32emu.h>

#include "utility.h"

class CROMManager
{
public:
	#define ENUM_ROMSET(ENUM) \
		ENUM(MT32Old, old)    \
		ENUM(MT32New, new)    \
		ENUM(CM32L, cm32l)    \
		ENUM(Any, any)

	CONFIG_ENUM(TROMSet, ENUM_ROMSET);

	CROMManager();
	~CROMManager();

	bool ScanROMs();
	bool HaveROMSet(TROMSet ROMSet) const;
	bool GetROMSet(TROMSet ROMSet, const MT32Emu::ROMImage*& pOutControl, const MT32Emu::ROMImage*& pOutPCM) const;

private:
	bool CheckROM(const char* pPath);
	bool StoreROM(const MT32Emu::ROMImage& ROMImage);

	// Control ROMs
	const MT32Emu::ROMImage* m_pMT32OldControl;
	const MT32Emu::ROMImage* m_pMT32NewControl;
	const MT32Emu::ROMImage* m_pCM32LControl;

	// PCM ROMs
	const MT32Emu::ROMImage* m_pMT32PCM;
	const MT32Emu::ROMImage* m_pCM32LPCM;
};

#endif
