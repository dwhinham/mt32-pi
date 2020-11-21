//
// soundfontmanager.cpp
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
#include <circle/util.h>
#include <fatfs/ff.h>

#include <fluidsynth.h>

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

	DIR dir;
	FILINFO fileInfo;
	FRESULT result = f_findfirst(&dir, &fileInfo, SoundFontPath, "*");

	char path[sizeof(SoundFontPath) + FF_LFN_BUF];
	strcpy(path, SoundFontPath);
	path[sizeof(SoundFontPath) - 1] = '/';

	// Loop over each file in the directory
	while (result == FR_OK && *fileInfo.fname && m_nSoundFonts < MaxSoundFonts)
	{
		// Ensure not directory, hidden, or system file
		if (!(fileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
		{
			// Assemble path
			strcpy(path + sizeof(SoundFontPath), fileInfo.fname);

			CheckSoundFont(path);
		}

		result = f_findnext(&dir, &fileInfo);
	}

	CLogger::Get()->Write(SoundFontManagerName, LogNotice, "%d SoundFonts found:", m_nSoundFonts);

	// Sort into alphabetical order
	if (m_nSoundFonts > 0)
		Utility::QSort(m_SoundFontList, SoundFontListComparator, 0, m_nSoundFonts - 1);

	for (size_t i = 0; i < m_nSoundFonts; ++i)
		CLogger::Get()->Write(SoundFontManagerName, LogNotice, "\t%s (%s)", static_cast<const char*>(m_SoundFontList[i].Path), static_cast<const char*>(m_SoundFontList[i].Name));

	return m_nSoundFonts > 0;
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

void CSoundFontManager::CheckSoundFont(const char* pPath)
{
	FIL pFile;
	UINT nBytesRead;
	TSoundFontChunk Chunk;
	u32 nFourCC;
	u32 nInfoListChunkSize;
	char Name[MaxSoundFontNameLength];

	// Init with null terminator
	Name[0] = '\0';

	// Try to open file
	if (f_open(&pFile, pPath, FA_READ) != FR_OK)
		return;

#define CHECK_CHUNK_ID(EXPECTED_CHUNK_ID)                                                                 \
	if (f_read(&pFile, &Chunk, sizeof(Chunk), &nBytesRead) != FR_OK || Chunk.FourCC != EXPECTED_CHUNK_ID) \
	{                                                                                                     \
		f_close(&pFile);                                                                                  \
		return;                                                                                           \
	}

#define CHECK_FORM_ID(EXPECTED_FORM_ID)                                                                 \
	if (f_read(&pFile, &nFourCC, sizeof(nFourCC), &nBytesRead) != FR_OK || nFourCC != EXPECTED_FORM_ID) \
	{                                                                                                   \
		f_close(&pFile);                                                                                \
		return;                                                                                         \
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

	while (nTotalBytesRead < nInfoListChunkSize && f_read(&pFile, &Chunk, sizeof(Chunk), &nBytesRead) == FR_OK)
	{
		nTotalBytesRead += nBytesRead;

		// Extract name
		if (Chunk.FourCC == FourCCINAM)
		{
			if (Chunk.Size <= sizeof(Name))
				f_read(&pFile, Name, Chunk.Size, &nBytesRead);

			break;
		}

		// Skip to start of next chunk
		else
			f_lseek(&pFile, f_tell(&pFile) + Chunk.Size);

		nTotalBytesRead += Chunk.Size;
	}

	// Clean up
	f_close(&pFile);

	TSoundFontListEntry& entry = m_SoundFontList[m_nSoundFonts++];
	entry.Path  = pPath;

	// If we got a name, use it
	// TODO: fall back on filename
	if (Name[0] != '\0')
		entry.Name = Name;
}

inline bool CSoundFontManager::SoundFontListComparator(const TSoundFontListEntry& lhs, const TSoundFontListEntry& rhs)
{
	return strcasecmp(lhs.Path, rhs.Path) < 0;
}
