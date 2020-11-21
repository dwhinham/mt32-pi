//
// soundfontmanager.h
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

#ifndef _soundfontmanager_h
#define _soundfontmanager_h

#include <circle/string.h>

class CSoundFontManager
{
public:
	CSoundFontManager();
	~CSoundFontManager() = default;

	bool ScanSoundFonts();
	const char* GetSoundFontPath(size_t nIndex) const;
	const char* GetSoundFontName(size_t nIndex) const;
	const char* GetFirstValidSoundFontPath() const;

private:
	struct TSoundFontListEntry
	{
		CString Name;
		CString Path;
	};

	static constexpr size_t MaxSoundFonts = 256;
	static constexpr size_t MaxSoundFontNameLength = 256;

	void CheckSoundFont(const char* pPath);

	size_t m_nSoundFonts;
	TSoundFontListEntry m_SoundFontList[MaxSoundFonts];

	inline static bool SoundFontListComparator(const TSoundFontListEntry& lhs, const TSoundFontListEntry& rhs);
};

#endif
