//
// rommanager.cpp
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
#include <fatfs/ff.h>

#include "rommanager.h"

const char ROMManagerName[] = "rommanager";
const char ROMPath[] = "roms";

// Filenames for original ROM loading behaviour
const char MT32ControlROMName[] = "MT32_CONTROL.ROM";
const char MT32PCMROMName[] = "MT32_PCM.ROM";

// Custom File class for mt32emu
class CROMFile : public MT32Emu::AbstractFile
{
public:
	CROMFile() : m_File{}, m_pData(nullptr) {}

	virtual ~CROMFile() override { close(); }

	virtual size_t getSize() override { return f_size(&m_File); }

	virtual const MT32Emu::Bit8u* getData() override { return m_pData; }

	virtual bool open(const char* pFileName)
	{
		FRESULT Result = f_open(&m_File, pFileName, FA_READ);
		if (Result != FR_OK)
			return false;

		FSIZE_t nSize = f_size(&m_File);
		m_pData       = new MT32Emu::Bit8u[nSize];

		UINT nRead;
		Result = f_read(&m_File, m_pData, nSize, &nRead);

		return Result == FR_OK;
	}

	virtual void close() override
	{
		f_close(&m_File);
		if (m_pData)
		{
			delete[] m_pData;
			m_pData = nullptr;
		}
	}

private:
	FIL m_File;
	MT32Emu::Bit8u* m_pData;
};

CROMManager::CROMManager()
	: m_pMT32OldControl(nullptr),
	  m_pMT32NewControl(nullptr),
	  m_pCM32LControl(nullptr),

	  m_pMT32PCM(nullptr),
	  m_pCM32LPCM(nullptr)
{
}

CROMManager::~CROMManager()
{
	const MT32Emu::ROMImage** const roms[] = { &m_pMT32OldControl, &m_pMT32NewControl, &m_pCM32LControl, &m_pMT32PCM, &m_pCM32LPCM };
	for (const MT32Emu::ROMImage** rom : roms)
	{
		if (*rom)
		{
			if (MT32Emu::File* File = (*rom)->getFile())
				delete File;
			MT32Emu::ROMImage::freeROMImage(*rom);
		}
	}
}

bool CROMManager::ScanROMs()
{
	DIR dir;
	FILINFO fileInfo;
	FRESULT result = f_findfirst(&dir, &fileInfo, ROMPath, "*");

	char path[sizeof(ROMPath) + FF_LFN_BUF];
	strcpy(path, ROMPath);
	path[sizeof(ROMPath) - 1] = '/';

	// Loop over each file in the directory
	while (result == FR_OK && *fileInfo.fname)
	{
		// Ensure not directory, hidden, or system file
		if (!(fileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
		{
			// Assemble path
			strcpy(path + sizeof(ROMPath), fileInfo.fname);

			// Try to open file
			CheckROM(path);
		}

		result = f_findnext(&dir, &fileInfo);
	}

	// Fall back on old ROM loading behavior if we haven't found at least one valid ROM set
	if (!HaveROMSet(TROMSet::Any))
		return CheckROM(MT32ControlROMName) && CheckROM(MT32PCMROMName);

	return true;
}

bool CROMManager::HaveROMSet(TROMSet ROMSet) const
{
	switch (ROMSet)
	{
		case TROMSet::Any:
			return (m_pMT32OldControl || m_pMT32NewControl || m_pCM32LControl) && (m_pMT32PCM || m_pCM32LPCM);

		case TROMSet::MT32Old:
			return m_pMT32OldControl && m_pMT32PCM;

		case TROMSet::MT32New:
			return m_pMT32NewControl && m_pMT32PCM;

		case TROMSet::CM32L:
			return m_pCM32LControl && m_pCM32LPCM;
	}

	return false;
}

bool CROMManager::GetROMSet(TROMSet ROMSet, const MT32Emu::ROMImage*& pOutControl, const MT32Emu::ROMImage*& pOutPCM) const
{
	if (!HaveROMSet(ROMSet))
		return false;

	switch (ROMSet)
	{
		case TROMSet::Any:
			if (m_pMT32OldControl)
				pOutControl = m_pMT32OldControl;
			else if (m_pMT32NewControl)
				pOutControl = m_pMT32NewControl;
			else
				pOutControl = m_pCM32LControl;

			if (pOutControl == m_pCM32LControl && m_pCM32LPCM)
				pOutPCM = m_pCM32LPCM;
			else
				pOutPCM = m_pMT32PCM;

			break;

		case TROMSet::MT32Old:
			pOutControl = m_pMT32OldControl;
			pOutPCM     = m_pMT32PCM;
			break;

		case TROMSet::MT32New:
			pOutControl = m_pMT32NewControl;
			pOutPCM     = m_pMT32PCM;
			break;

		case TROMSet::CM32L:
			pOutControl = m_pCM32LControl;
			pOutPCM     = m_pCM32LPCM;
			break;
	}

	return true;
}

bool CROMManager::CheckROM(const char* pPath)
{
	CROMFile* file = new CROMFile();
	if (!file->open(pPath))
	{
		CLogger::Get()->Write(ROMManagerName, LogError, "Couldn't open '%s' for reading", pPath);
		delete file;
		return false;
	}

	// Check ROM and store if valid
	const MT32Emu::ROMImage* rom = MT32Emu::ROMImage::makeROMImage(file);
	if (!StoreROM(*rom))
	{
		MT32Emu::ROMImage::freeROMImage(rom);
		delete file;
		return false;
	}

	return true;
}

bool CROMManager::StoreROM(const MT32Emu::ROMImage& ROMImage)
{
	const MT32Emu::ROMInfo* romInfo = ROMImage.getROMInfo();
	const MT32Emu::ROMImage** romPtr = nullptr;

	// Not a valid ROM file
	if (!romInfo)
		return false;

	if (romInfo->type == MT32Emu::ROMInfo::Type::Control)
	{
		// Is an 'old' MT-32 control ROM
		if (romInfo->shortName[10] == '1' || romInfo->shortName[10] == 'b')
			romPtr = &m_pMT32OldControl;

		// Is a 'new' MT-32 control ROM
		else if (romInfo->shortName[10] == '2')
			romPtr = &m_pMT32NewControl;

		// Is a CM-32L control ROM
		else
			romPtr = &m_pCM32LControl;
	}
	else if (romInfo->type == MT32Emu::ROMInfo::Type::PCM)
	{
		// Is an MT-32 PCM ROM
		if (romInfo->shortName[4] == 'm')
			romPtr = &m_pMT32PCM;

		// Is a CM-32L PCM ROM
		else
			romPtr = &m_pCM32LPCM;
	}

	// Ensure we don't already have this ROM
	if (!romPtr || *romPtr)
		return false;

	*romPtr = &ROMImage;
	return true;
}
