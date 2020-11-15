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

CSoundFontManager::CSoundFontManager()
	: m_nSoundFonts(0)
{
}

bool CSoundFontManager::ScanSoundFonts()
{
	// Clear existing SoundFont names
	for (size_t i = 0; i < m_nSoundFonts; ++i)
		m_SoundFontPaths[i] = CString();

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
		Utility::QSort(m_SoundFontPaths, Utility::Comparator::CaseInsensitiveAscending, 0, m_nSoundFonts - 1);

	for (size_t i = 0; i < m_nSoundFonts; ++i)
		CLogger::Get()->Write(SoundFontManagerName, LogNotice, "\t%s", static_cast<const char*>(m_SoundFontPaths[i]));

	return m_nSoundFonts > 0;
}

const char* CSoundFontManager::GetSoundFontPath(size_t nIndex) const
{
	// Return the path if in-range
	return nIndex < m_nSoundFonts ? static_cast<const char*>(m_SoundFontPaths[nIndex]) : nullptr;
}

const char* CSoundFontManager::GetFirstValidSoundFontPath() const
{
	return m_nSoundFonts > 0 ? static_cast<const char*>(m_SoundFontPaths[0]) : nullptr;
}

void CSoundFontManager::CheckSoundFont(const char* pPath)
{
	// Try to open file
	if (fluid_is_soundfont(pPath))
		m_SoundFontPaths[m_nSoundFonts++] = pPath;
}
