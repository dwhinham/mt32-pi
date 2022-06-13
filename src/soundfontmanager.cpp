//
// soundfontmanager.cpp
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
#include <circle/util.h>
#include <fatfs/ff.h>

#include <ini.h>

#include "config.h"
#include "soundfontmanager.h"
#include "utility.h"

const char SoundFontManagerName[] = "soundfontmanager";
const char* const Disks[] = { "SD", "USB" };
const char SoundFontDirectory[] = "soundfonts";

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

	m_nSoundFonts = 0;

	DIR Dir;
	FILINFO FileInfo;
	FRESULT Result;
	CString DirectoryPath;

	// Loop over each disk
	for (auto pDisk : Disks)
	{
		DirectoryPath.Format("%s:%s", pDisk, SoundFontDirectory);
		Result = f_findfirst(&Dir, &FileInfo, DirectoryPath, "*");

		// Loop over each file in the directory
		while (Result == FR_OK && *FileInfo.fname && m_nSoundFonts < MaxSoundFonts)
		{
			// Ensure not directory, hidden, or system file
			if (!(FileInfo.fattrib & (AM_DIR | AM_HID | AM_SYS)))
			{
				// Assemble path
				CString SoundFontPath;
				SoundFontPath.Format("%s/%s", static_cast<const char*>(DirectoryPath), FileInfo.fname);

				CheckSoundFont(SoundFontPath, FileInfo.fname);
			}

			Result = f_findnext(&Dir, &FileInfo);
		}
	}

	if (m_nSoundFonts > 0)
	{
		// Sort into lexicographical order
		Utility::QSort(m_SoundFontList, SoundFontListComparator, 0, m_nSoundFonts - 1);

		CLogger* const pLogger = CLogger::Get();
		pLogger->Write(SoundFontManagerName, LogNotice, "%d SoundFonts found:", m_nSoundFonts);
		for (size_t i = 0; i < m_nSoundFonts; ++i)
			pLogger->Write(SoundFontManagerName, LogNotice, "%d: %s (%s)", i, static_cast<const char*>(m_SoundFontList[i].Path), static_cast<const char*>(m_SoundFontList[i].Name));

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

TFXProfile CSoundFontManager::GetSoundFontFXProfile(size_t nIndex) const
{
	TFXProfile FXProfile;

	const char* const pSoundFontPath = GetSoundFontPath(nIndex);
	if (!pSoundFontPath)
		return FXProfile;

	// +5 bytes in case we need to add an extension (4 chars + null terminator)
	const size_t nPathLength = strlen(pSoundFontPath) + 5;
	char PathBuffer[nPathLength];
	strcpy(PathBuffer, pSoundFontPath);

	// Replace file extension if present
	char* const pExtension = strrchr(PathBuffer, '.');
	if (pExtension)
		strcpy(pExtension, ".cfg");
	else
		strcat(PathBuffer, ".cfg");

	FIL File;
	if (f_open(&File, PathBuffer, FA_READ) != FR_OK)
		return FXProfile;

	// +1 byte for null terminator
	const UINT nSize = f_size(&File);
	char Buffer[nSize + 1];
	UINT nRead;

	if (f_read(&File, Buffer, nSize, &nRead) != FR_OK)
	{
		CLogger::Get()->Write(SoundFontManagerName, LogError, "Error reading effects profile");
		f_close(&File);
		return FXProfile;
	}

	// Ensure null-terminated
	Buffer[nRead] = '\0';

	const int nResult = ini_parse_string(Buffer, INIHandler, &FXProfile);
	if (nResult > 0)
		CLogger::Get()->Write(SoundFontManagerName, LogWarning, "Effects profile parse error on line %d", nResult);

	f_close(&File);
	return FXProfile;
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

	TSoundFontListEntry& Entry = m_SoundFontList[m_nSoundFonts++];
	Entry.Path = pFullPath;

	// If we got a name, use it, otherwise fall back on filename
	if (Name[0] != '\0')
		Entry.Name = Name;
	else
		Entry.Name = pFileName;
}

inline bool CSoundFontManager::SoundFontListComparator(const TSoundFontListEntry& EntryA, const TSoundFontListEntry& EntryB)
{
	return strcasecmp(EntryA.Path, EntryB.Path) < 0;
}

int CSoundFontManager::INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue)
{
	TFXProfile* const pFXProfile = static_cast<TFXProfile*>(pUser);

	#define MATCH(NAME, TYPE, STRUCT_MEMBER)                            \
		if (!strcmp(NAME, pName))                                   \
		{                                                           \
			auto Temp = decltype(*pFXProfile->STRUCT_MEMBER){}; \
			if (CConfig::ParseOption(pValue, &Temp))            \
			{                                                   \
				pFXProfile->STRUCT_MEMBER = Temp;           \
				return 1;                                   \
			}                                                   \
			return 0;                                           \
		}

	MATCH("gain", float, nGain);

	MATCH("reverb", bool, bReverbActive);
	MATCH("reverb_damping", float, nReverbDamping);
	MATCH("reverb_level", float, nReverbLevel);
	MATCH("reverb_room_size", float, nReverbRoomSize);
	MATCH("reverb_width", float, nReverbWidth);

	MATCH("chorus", bool, bChorusActive);
	MATCH("chorus_depth", float, nChorusDepth);
	MATCH("chorus_level", float, nChorusLevel);
	MATCH("chorus_voices", int, nChorusVoices);
	MATCH("chorus_speed", float, nChorusSpeed);

	#undef MATCH
	return 0;
}
