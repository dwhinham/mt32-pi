//
// soundfontmanager.h
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

#ifndef _soundfontmanager_h
#define _soundfontmanager_h

#include <circle/string.h>

#include "synth/fxprofile.h"

class CSoundFontManager
{
public:
	CSoundFontManager();
	~CSoundFontManager() = default;

	bool ScanSoundFonts();
	size_t GetSoundFontCount() const { return m_nSoundFonts; }
	const char* GetSoundFontPath(size_t nIndex) const;
	const char* GetSoundFontName(size_t nIndex) const;
	TFXProfile GetSoundFontFXProfile(size_t nIndex) const;
	const char* GetFirstValidSoundFontPath() const;

	static constexpr size_t MaxSoundFonts = 512;

private:
	struct TSoundFontListEntry
	{
		CString Name;
		CString Path;
	};

	static constexpr size_t MaxSoundFontNameLength = 256;

	void CheckSoundFont(const char* pFullPath, const char* pFileName);

	size_t m_nSoundFonts;
	TSoundFontListEntry m_SoundFontList[MaxSoundFonts];

	static int INIHandler(void* pUser, const char* pSection, const char* pName, const char* pValue);
	inline static bool SoundFontListComparator(const TSoundFontListEntry& lhs, const TSoundFontListEntry& rhs);
};

#endif
