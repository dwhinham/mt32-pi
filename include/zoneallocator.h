//
// zoneallocator.h
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

#ifndef _zoneallocator_h
#define _zoneallocator_h

#include <circle/types.h>

// Block allocation tags
enum TZoneTag : u32
{
	Free = 0,
	Uncategorized = 1,
	FluidSynth
};

class CZoneAllocator
{
public:
	CZoneAllocator();
	~CZoneAllocator();

	// Allocator interface
	bool Initialize();
	void* Alloc(size_t nSize, TZoneTag Tag);
	void* Realloc(void* pPtr, size_t nSize, TZoneTag Tag);
	void Free(void* pPtr);
	size_t GetAllocCount() const { return m_nAllocCount; }

	void FreeTag(u32 nTag);
	void Clear();
	void Dump() const;

	static CZoneAllocator* Get() { return s_pThis; }

private:
	// Memory block header/linked list
	struct TBlock
	{
		size_t nSize;      // 32bit: 4  bytes  |  64bit: 8  bytes
		TBlock* pNext;     //        8  bytes  |         16 bytes
		TBlock* pPrevious; //        12 bytes  |         24 bytes
		TZoneTag Tag;      //        16 bytes  |         28 bytes
		u32 nMagic;        //        20 bytes  |         32 bytes
#if AARCH == 32
		u8 Padding[12]; //        32 bytes  |
#endif
	};

	// Constants
	static constexpr u32 BlockMagic         = 0xDA1EDEAD;
	static constexpr size_t MinFragmentSize = 16;

	inline u32& GetEndMagic(TBlock* pBlock) const
	{
		return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(pBlock) + pBlock->nSize - sizeof(BlockMagic));
	}

	void* m_pHeap;
	size_t m_nHeapSize;
	TBlock m_MainBlock;
	TBlock* m_pCurrentBlock;

	size_t m_nAllocCount;

	static CZoneAllocator* s_pThis;
};

#endif
