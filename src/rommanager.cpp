//
// rommanager.cpp
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
#include <fatfs/ff.h>

#include "rommanager.h"

const char ROMManagerName[] = "rommanager";
const char* const Disks[] = { "SD", "USB" };
const char ROMDirectory[] = "roms";

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
		if (nSize > MaxROMFileSize)
			return false;

		if (!(m_pData = new MT32Emu::Bit8u[nSize]))
			return false;

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
	// The largest ROM is the CM-32L PCM ROM at 1MB; files larger than this cannot be valid
	static constexpr size_t MaxROMFileSize = 1 * MEGABYTE;

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
	const MT32Emu::ROMImage** const ROMs[] = { &m_pMT32OldControl, &m_pMT32NewControl, &m_pCM32LControl, &m_pMT32PCM, &m_pCM32LPCM };
	for (const MT32Emu::ROMImage** pROMImagePtr : ROMs)
	{
		if (*pROMImagePtr)
		{
			if (MT32Emu::File* File = (*pROMImagePtr)->getFile())
				delete File;
			MT32Emu::ROMImage::freeROMImage(*pROMImagePtr);
		}
	}
}

bool CROMManager::ScanROMs()
{
	DIR Dir;
	FILINFO FileInfo;
	FRESULT Result;
	CString DirectoryPath;

	// Already have all ROMs
	if (HaveROMSet(TMT32ROMSet::All))
		return true;

	// Loop over each disk
	for (auto pDisk : Disks)
	{
		DirectoryPath.Format("%s:/%s", pDisk, ROMDirectory);
		Result = f_findfirst(&Dir, &FileInfo, DirectoryPath, "*");

		// Loop over each file in the directory
		while (Result == FR_OK && *FileInfo.fname)
		{
			// Ensure not directory, hidden, or system file
			if (!(FileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
			{
				// Assemble path
				CString ROMPath(static_cast<const char*>(DirectoryPath));
				ROMPath.Append("/");
				ROMPath.Append(FileInfo.fname);

				// Try to open file
				CheckROM(ROMPath);

				// Stop if we have all ROMs
				if (HaveROMSet(TMT32ROMSet::All))
					return true;
			}

			Result = f_findnext(&Dir, &FileInfo);
		}
	}

	return HaveROMSet(TMT32ROMSet::Any);
}

bool CROMManager::HaveROMSet(TMT32ROMSet ROMSet) const
{
	switch (ROMSet)
	{
		case TMT32ROMSet::Any:
			return ((m_pMT32OldControl || m_pMT32NewControl) && m_pMT32PCM) || (m_pCM32LControl && m_pCM32LPCM);

		case TMT32ROMSet::All:
			return m_pMT32OldControl && m_pMT32NewControl && m_pCM32LControl && m_pMT32PCM && m_pCM32LPCM;

		case TMT32ROMSet::MT32Old:
			return m_pMT32OldControl && m_pMT32PCM;

		case TMT32ROMSet::MT32New:
			return m_pMT32NewControl && m_pMT32PCM;

		case TMT32ROMSet::CM32L:
			return m_pCM32LControl && m_pCM32LPCM;
	}

	return false;
}

bool CROMManager::GetROMSet(TMT32ROMSet ROMSet, TMT32ROMSet& pOutROMSet, const MT32Emu::ROMImage*& pOutControl, const MT32Emu::ROMImage*& pOutPCM) const
{
	if (!HaveROMSet(ROMSet))
		return false;

	switch (ROMSet)
	{
		case TMT32ROMSet::Any:
			if (m_pMT32OldControl)
			{
				pOutControl = m_pMT32OldControl;
				pOutROMSet  = TMT32ROMSet::MT32Old;
			}
			else if (m_pMT32NewControl)
			{
				pOutControl = m_pMT32NewControl;
				pOutROMSet  = TMT32ROMSet::MT32New;
			}
			else
			{
				pOutControl = m_pCM32LControl;
				pOutROMSet  = TMT32ROMSet::CM32L;
			}

			if (pOutControl == m_pCM32LControl)
				pOutPCM = m_pCM32LPCM;
			else
				pOutPCM = m_pMT32PCM;

			break;

		case TMT32ROMSet::MT32Old:
			pOutControl = m_pMT32OldControl;
			pOutPCM     = m_pMT32PCM;
			pOutROMSet  = TMT32ROMSet::MT32Old;
			break;

		case TMT32ROMSet::MT32New:
			pOutControl = m_pMT32NewControl;
			pOutPCM     = m_pMT32PCM;
			pOutROMSet  = TMT32ROMSet::MT32New;
			break;

		case TMT32ROMSet::CM32L:
			pOutControl = m_pCM32LControl;
			pOutPCM     = m_pCM32LPCM;
			pOutROMSet  = TMT32ROMSet::CM32L;
			break;

		default:
			return false;
	}

	return true;
}

bool CROMManager::CheckROM(const char* pPath)
{
	CROMFile* pFile = new CROMFile();
	if (!pFile->open(pPath))
	{
		CLogger::Get()->Write(ROMManagerName, LogError, "Couldn't open '%s' for reading", pPath);
		delete pFile;
		return false;
	}

	// Check ROM and store if valid
	const MT32Emu::ROMImage* pROM = MT32Emu::ROMImage::makeROMImage(pFile);
	if (!StoreROM(*pROM))
	{
		MT32Emu::ROMImage::freeROMImage(pROM);
		delete pFile;
		return false;
	}

	return true;
}

bool CROMManager::StoreROM(const MT32Emu::ROMImage& ROMImage)
{
	const MT32Emu::ROMInfo* pROMInfo = ROMImage.getROMInfo();
	const MT32Emu::ROMImage** pROMImagePtr = nullptr;

	// Not a valid ROM file
	if (!pROMInfo)
		return false;

	if (pROMInfo->type == MT32Emu::ROMInfo::Type::Control)
	{
		// Is an 'old' MT-32 control ROM
		if (pROMInfo->shortName[10] == '1' || pROMInfo->shortName[10] == 'b')
			pROMImagePtr = &m_pMT32OldControl;

		// Is a 'new' MT-32 control ROM
		else if (pROMInfo->shortName[10] == '2')
			pROMImagePtr = &m_pMT32NewControl;

		// Is a CM-32L control ROM
		else
			pROMImagePtr = &m_pCM32LControl;
	}
	else if (pROMInfo->type == MT32Emu::ROMInfo::Type::PCM)
	{
		// Is an MT-32 PCM ROM
		if (pROMInfo->shortName[4] == 'm')
			pROMImagePtr = &m_pMT32PCM;

		// Is a CM-32L PCM ROM
		else
			pROMImagePtr = &m_pCM32LPCM;
	}

	// Ensure we don't already have this ROM
	if (!pROMImagePtr || *pROMImagePtr)
		return false;

	*pROMImagePtr = &ROMImage;
	return true;
}
