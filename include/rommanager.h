//
// rommanager.h
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

#ifndef _rommanager_h
#define _rommanager_h

#include <mt32emu/mt32emu.h>

#include "synth/mt32romset.h"

class CROMManager
{
public:
	CROMManager();
	~CROMManager();

	bool ScanROMs();
	bool HaveROMSet(TMT32ROMSet ROMSet) const;
	bool GetROMSet(TMT32ROMSet ROMSet, TMT32ROMSet& pOutROMSet, const MT32Emu::ROMImage*& pOutControl, const MT32Emu::ROMImage*& pOutPCM) const;

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
