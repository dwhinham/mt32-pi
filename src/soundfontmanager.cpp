//
// soundfontmanager.cpp
//
// mt32-pi - A baremetal MIDI synthesizer for Raspberry Pi
// Copyright (C) 2020-2021 Dale Whinham <daleyo@gmail.com>
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
#include <circle/util.h>
#include <fatfs/ff.h>

#include "soundfontmanager.h"
#include "utility.h"

const char SoundFontManagerName[] = "soundfontmanager";
const char SoundFontPath[] = "soundfonts";

// Four-character codes used throughout SoundFont RIFF structure
constexpr u32 FourCC(const char pFourCC[4])
{
	return pFourCC[3] << 24 | pFourCC[2] << 16 | pFourCC[1] << 8 | pFourCC[0];
}

constexpr u32 FourCCINAM = FourCC("INAM");
constexpr u32 FourCCINFO = FourCC("INFO");
constexpr u32 FourCCLIST = FourCC("LIST");
constexpr u32 FourCCRIFF = FourCC("RIFF");
constexpr u32 FourCCSFBK = FourCC("sfbk");

struct TSoundFontChunk
{
	u32 FourCC;
	u32 Size;
}
PACKED;

CSoundFontManager::CSoundFontManager()
	: m_nSoundFonts(0)
{
}

bool CSoundFontManager::ScanSoundFonts()
{
	// Clear existing SoundFont list entries
	for (size_t i = 0; i < m_nSoundFonts; ++i)
		m_SoundFontList[i] = TSoundFontListEntry();

	DIR Dir;
	FILINFO FileInfo;
	FRESULT Result = f_findfirst(&Dir, &FileInfo, SoundFontPath, "*");

	char Path[sizeof(SoundFontPath) + FF_LFN_BUF];
	strcpy(Path, SoundFontPath);
	Path[sizeof(SoundFontPath) - 1] = '/';

	// Loop over each file in the directory
	while (Result == FR_OK && *FileInfo.fname && m_nSoundFonts < MaxSoundFonts)
	{
		// Ensure not directory, hidden, or system file
		if (!(FileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
		{
			// Assemble path
			strcpy(Path + sizeof(SoundFontPath), FileInfo.fname);

			CheckSoundFont(Path, FileInfo.fname);
		}

		Result = f_findnext(&Dir, &FileInfo);
	}

	// Sort into alphabetical order
	if (m_nSoundFonts > 0)
	{
		// Sort into lexicographical order
		Utility::QSort(m_SoundFontList, SoundFontListComparator, 0, m_nSoundFonts - 1);

		CLogger& Logger = *CLogger::Get();
		Logger.Write(SoundFontManagerName, LogNotice, "%d SoundFonts found:", m_nSoundFonts);
		for (size_t i = 0; i < m_nSoundFonts; ++i)
			Logger.Write(SoundFontManagerName, LogNotice, "%d: %s (%s)", i, static_cast<const char*>(m_SoundFontList[i].Path), static_cast<const char*>(m_SoundFontList[i].Name));

		return true;
	}

	return false;
}

const char* CSoundFontManager::GetSoundFontPath(size_t nIndex) const
{
	// Return the path if in-range
	return nIndex < m_nSoundFonts ? static_cast<const char*>(m_SoundFontList[nIndex].Path) : nullptr;
}

const char* CSoundFontManager::GetSoundFontName(size_t nIndex) const
{
	// Out of range
	if (nIndex >= m_nSoundFonts)
		return nullptr;

	// If name empty, return path
	if (m_SoundFontList[nIndex].Name.GetLength() == 0)
		return static_cast<const char*>(m_SoundFontList[nIndex].Path);

	return static_cast<const char*>(m_SoundFontList[nIndex].Name);
}

const char* CSoundFontManager::GetFirstValidSoundFontPath() const
{
	return m_nSoundFonts > 0 ? static_cast<const char*>(m_SoundFontList[0].Path) : nullptr;
}

void CSoundFontManager::CheckSoundFont(const char* pFullPath, const char* pFileName)
{
	FIL File;
	UINT nBytesRead;
	TSoundFontChunk Chunk;
	u32 nFourCC;
	u32 nInfoListChunkSize;
	char Name[MaxSoundFontNameLength];

	// Init with null terminator
	Name[0] = '\0';

	// Try to open file
	if (f_open(&File, pFullPath, FA_READ) != FR_OK)
		return;

#define CHECK_CHUNK_ID(EXPECTED_CHUNK_ID)                                                                \
	if (f_read(&File, &Chunk, sizeof(Chunk), &nBytesRead) != FR_OK || Chunk.FourCC != EXPECTED_CHUNK_ID) \
	{                                                                                                    \
		f_close(&File);                                                                                  \
		return;                                                                                          \
	}

#define CHECK_FORM_ID(EXPECTED_FORM_ID)                                                                \
	if (f_read(&File, &nFourCC, sizeof(nFourCC), &nBytesRead) != FR_OK || nFourCC != EXPECTED_FORM_ID) \
	{                                                                                                  \
		f_close(&File);                                                                                \
		return;                                                                                        \
	}

	CHECK_CHUNK_ID(FourCCRIFF);
	CHECK_FORM_ID(FourCCSFBK);
	CHECK_CHUNK_ID(FourCCLIST);
	CHECK_FORM_ID(FourCCINFO);

	#undef CHECK_CHUNK_ID
	#undef CHECK_FORM_ID

	// Loop over info list chunks and look for name chunk
	nInfoListChunkSize = Chunk.Size;
	size_t nTotalBytesRead = 4;

	while (nTotalBytesRead < nInfoListChunkSize && f_read(&File, &Chunk, sizeof(Chunk), &nBytesRead) == FR_OK)
	{
		nTotalBytesRead += nBytesRead;

		// Extract name
		if (Chunk.FourCC == FourCCINAM)
		{
			if (Chunk.Size <= sizeof(Name))
				f_read(&File, Name, Chunk.Size, &nBytesRead);

			break;
		}

		// Skip to start of next chunk
		else
			f_lseek(&File, f_tell(&File) + Chunk.Size);

		nTotalBytesRead += Chunk.Size;
	}

	// Clean up
	f_close(&File);

	TSoundFontListEntry& entry = m_SoundFontList[m_nSoundFonts++];
	entry.Path  = pFullPath;

	// If we got a name, use it, otherwise fall back on filename
	if (Name[0] != '\0')
		entry.Name = Name;
	else
		entry.Name = pFileName;
}

inline bool CSoundFontManager::SoundFontListComparator(const TSoundFontListEntry& lhs, const TSoundFontListEntry& rhs)
{
	return strcasecmp(lhs.Path, rhs.Path) < 0;
}
