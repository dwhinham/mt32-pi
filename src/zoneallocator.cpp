//
// zoneallocator.cpp
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

#include <circle/alloc.h>
#include <circle/logger.h>
#include <circle/memory.h>

#include "utility.h"
#include "zoneallocator.h"

constexpr size_t MallocHeapSize = 32 * MEGABYTE;

const char ZoneAllocatorName[] = "zoneallocator";

// #define ZONE_ALLOCATOR_DEBUG
// #define ZONE_ALLOCATOR_TRACE

CZoneAllocator* CZoneAllocator::s_pThis = nullptr;

CZoneAllocator::CZoneAllocator()
	: m_pHeap(nullptr),
	  m_nHeapSize(0),
	  m_pCurrentBlock(nullptr),
	  m_nAllocCount(0)
{
	assert(s_pThis == nullptr);
	s_pThis = this;
}

CZoneAllocator::~CZoneAllocator()
{
	// Release the entire heap
	CMemorySystem::Get()->HeapFree(m_pHeap);
}

bool CZoneAllocator::Initialize()
{
	CMemorySystem* pMemorySystem = CMemorySystem::Get();
	CLogger* pLogger = CLogger::Get();

#if RASPI >= 4
	const size_t nHighHeapSize = pMemorySystem->GetHeapFreeSpace(HEAP_HIGH);
	if (nHighHeapSize)
	{
		// >1GB RAM Pi 4 - allocate all of the remaining HIGH region
		m_nHeapSize = nHighHeapSize - sizeof(THeapBlockHeader);
		m_pHeap     = pMemorySystem->HeapAllocate(m_nHeapSize, HEAP_HIGH);
	}
	else
	{
#endif
		// Allocate the majority of the remaining LOW region; leave some space for Circle/libc malloc()
		m_nHeapSize = pMemorySystem->GetHeapFreeSpace(HEAP_LOW) - MallocHeapSize;
		m_pHeap     = pMemorySystem->HeapAllocate(m_nHeapSize, HEAP_LOW);
#if RASPI >= 4
	}
#endif

	if (!m_pHeap)
	{
		if (m_nHeapSize >= MEGABYTE)
			pLogger->Write(ZoneAllocatorName, LogError, "Couldn't allocate a %d megabyte heap", m_nHeapSize / MEGABYTE);
		else
			pLogger->Write(ZoneAllocatorName, LogError, "Couldn't allocate a %d byte heap", m_nHeapSize);
		return false;
	}

	if (m_nHeapSize >= MEGABYTE)
		pLogger->Write(ZoneAllocatorName, LogNotice, "Allocated a %d megabyte heap at %p", m_nHeapSize / MEGABYTE, m_pHeap);
	else
		pLogger->Write(ZoneAllocatorName, LogNotice, "Allocated a %d byte heap at %p", m_nHeapSize, m_pHeap);

#ifdef ZONE_ALLOCATOR_DEBUG
	if ((reinterpret_cast<uintptr>(m_pHeap) & 15) == 0)
		pLogger->Write(ZoneAllocatorName, LogDebug, "Heap is 16-byte aligned");
	else
		pLogger->Write(ZoneAllocatorName, LogDebug, "Heap is NOT 16-byte aligned");

	pLogger->Write(ZoneAllocatorName, LogDebug, "Size of block header: %d", sizeof(TBlock));
#endif

	// Initialize the heap with an empty block
	Clear();

	return true;
}

void* CZoneAllocator::Alloc(size_t nSize, TZoneTag Tag)
{
	if (!nSize)
		return nullptr;

	if (Tag == TZoneTag::Free)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Zone allocation failed: tag value of 0 was used");
		return nullptr;
	}

	// Account for size of block header and magic number at end of zone (for corruption detection), padded to 16 bytes
	nSize = (nSize + sizeof(TBlock) + sizeof(BlockMagic) + 0xF) & ~0xF;

	TBlock* pNextBlock      = m_pCurrentBlock;
	TBlock* pCandidateBlock = m_pCurrentBlock;
	TBlock* pStartBlock     = m_pCurrentBlock->pPrevious;

	do
	{
		// We've been through the whole linked list and couldn't find a free block
		if (pNextBlock == pStartBlock)
		{
			CLogger::Get()->Write(ZoneAllocatorName, LogError, "Zone allocation failed: couldn't allocate %d bytes", nSize);
			return nullptr;
		}

		// This block is in use; look at the next one
		if (pNextBlock->Tag != TZoneTag::Free)
			pCandidateBlock = pNextBlock->pNext;

		pNextBlock = pNextBlock->pNext;
	} while (pCandidateBlock->Tag || pCandidateBlock->nSize < nSize);

	// Create a new block for any remaining free space
	const size_t nRemaining = pCandidateBlock->nSize - nSize;
	if (nRemaining > MinFragmentSize)
	{
		TBlock* pNewBlock    = reinterpret_cast<TBlock*>(reinterpret_cast<u8*>(pCandidateBlock) + nSize);
		pNewBlock->nSize     = nRemaining;
		pNewBlock->pNext     = pCandidateBlock->pNext;
		pNewBlock->pPrevious = pCandidateBlock;
		pNewBlock->Tag       = TZoneTag::Free;
		pNewBlock->nMagic    = BlockMagic;
#if AARCH == 32
		memset(pNewBlock->Padding, 0xEB, Utility::ArraySize(pNewBlock->Padding));
#endif
		// Set the next block's previous to look at the new block
		pNewBlock->pNext->pPrevious = pNewBlock;

		pCandidateBlock->nSize = nSize;
		pCandidateBlock->pNext = pNewBlock;
	}

	// Mark block used
	pCandidateBlock->Tag    = Tag;
	pCandidateBlock->nMagic = BlockMagic;

	// Mark end of memory with magic number
	GetEndMagic(pCandidateBlock) = BlockMagic;

	// Next allocation will start looking at this block
	m_pCurrentBlock = pCandidateBlock->pNext;

#ifdef ZONE_ALLOCATOR_TRACE
	CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Allocated %d bytes for tag %x", nSize, Tag);
#endif

	// Increment alloc counter
	++m_nAllocCount;

	return pCandidateBlock + 1;
}

void* CZoneAllocator::Realloc(void* pPtr, size_t nSize, TZoneTag Tag)
{
	// If passed a null pointer, perform a new allocation
	if (!pPtr)
		return Alloc(nSize, Tag);

	if (!nSize)
		return nullptr;

	// Account for size of block header and magic number at end of zone (for corruption detection), padded to 16 bytes
	const size_t nNewSize = (nSize + sizeof(TBlock) + sizeof(BlockMagic) + 0xF) & ~0xF;
	TBlock* pBlock        = reinterpret_cast<TBlock*>(pPtr) - 1;

	if (Tag == TZoneTag::Free)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Zone reallocation failed: tag value of 0 was used");
		return nullptr;
	}

	if (pBlock->Tag == TZoneTag::Free)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Attempted to reallocate a freed block");
		return nullptr;
	}

	// Expand block
	if (nNewSize > pBlock->nSize)
	{
		const size_t nSizeDiff = nNewSize - pBlock->nSize;

		// Expand in-place if next block is free and large enough
		if (pBlock->pNext->Tag == TZoneTag::Free && pBlock->pNext->nSize >= nSizeDiff)
		{
			TBlock* pNewBlock = reinterpret_cast<TBlock*>(reinterpret_cast<u8*>(pBlock) + nNewSize);

			pNewBlock->nSize            = pBlock->pNext->nSize - nSizeDiff;
			pNewBlock->pNext            = pBlock->pNext->pNext;
			pNewBlock->pNext->pPrevious = pNewBlock;
			pNewBlock->pPrevious        = pBlock;
			pNewBlock->Tag              = TZoneTag::Free;
			pNewBlock->nMagic           = BlockMagic;
#if AARCH == 32
			memset(pNewBlock->Padding, 0xEB, Utility::ArraySize(pNewBlock->Padding));
#endif
			GetEndMagic(pNewBlock) = BlockMagic;

			// Next allocations search from this new merged free block
			if (pBlock->pNext == m_pCurrentBlock)
				m_pCurrentBlock = pNewBlock;

			pBlock->nSize       = nNewSize;
			pBlock->pNext       = pNewBlock;
			pBlock->Tag         = Tag;
			GetEndMagic(pBlock) = BlockMagic;

#ifdef ZONE_ALLOCATOR_TRACE
			CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Expanded block at %p in-place", pPtr);
#endif

			return pBlock + 1;
		}

		// Allocate a new block and move contents
		else
		{
			const size_t nSrcSize = pBlock->nSize - sizeof(TBlock) - sizeof(BlockMagic);
			void* pDest           = Alloc(nSize, Tag);

			if (!pDest)
			{
				CLogger::Get()->Write(ZoneAllocatorName, LogError, "Zone reallocation failed");
				return nullptr;
			}

			memcpy(pDest, pPtr, nSrcSize);
			Free(pPtr);

#ifdef ZONE_ALLOCATOR_TRACE
			CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Expanded block at %p by allocating new block", pPtr);
#endif

			return pDest;
		}
	}

	// Shrink in-place
	if (nNewSize < pBlock->nSize)
	{
		const size_t nRemain = pBlock->nSize - nNewSize;
		if (nRemain > MinFragmentSize)
		{
			TBlock* pNewBlock = reinterpret_cast<TBlock*>(reinterpret_cast<u8*>(pBlock) + nNewSize);

			if (pBlock->pNext->Tag == TZoneTag::Free)
			{
				// Merge free space with next block if it is also free
				*pNewBlock = *pBlock->pNext;
				pNewBlock->nSize += nRemain;
#ifdef ZONE_ALLOCATOR_TRACE
				CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Shrunk block at %p in-place; adjacent free space expanded", pPtr);
#endif
			}
			else
			{
				// Create a new block for any remaining free space
				pNewBlock->nSize     = nRemain;
				pNewBlock->pNext     = pBlock->pNext;
				pNewBlock->pPrevious = pBlock;
				pNewBlock->Tag       = TZoneTag::Free;
				pNewBlock->nMagic    = BlockMagic;
#if AARCH == 32
				memset(pNewBlock->Padding, 0xEB, Utility::ArraySize(pNewBlock->Padding));
#endif
				GetEndMagic(pNewBlock) = BlockMagic;
#ifdef ZONE_ALLOCATOR_TRACE
				CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Shrunk block at %p in-place; new free block inserted after", pPtr);
#endif
			}

			// Next allocations search from this new merged free block
			if (pBlock->pNext == m_pCurrentBlock)
				m_pCurrentBlock = pNewBlock;

			// Set the next block's previous to look at the new block
			pNewBlock->pNext->pPrevious = pNewBlock;

			pBlock->pNext = pNewBlock;
		}

		pBlock->nSize = nNewSize;
		pBlock->Tag   = Tag;

		// Mark end of memory with magic number
		GetEndMagic(pBlock) = BlockMagic;

		return pBlock + 1;
	}

	// Size is the same, just update tag
	pBlock->Tag = Tag;
	return pPtr;
}

void CZoneAllocator::Free(void* pPtr)
{
	if (!pPtr)
		return;

	TBlock* pBlock = reinterpret_cast<TBlock*>(pPtr) - 1;

	if (pBlock->Tag == TZoneTag::Free)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Attempted to free an already-freed block");
		return;
	}

	if (pBlock->nMagic != BlockMagic)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Attempted to free a block with a bad magic number (heap corruption?)");
		return;
	}

	// Mark this block as free
	pBlock->Tag = TZoneTag::Free;

	// Join with previous block if previous block is also free
	TBlock* pAdjacentBlock = pBlock->pPrevious;
	if (pAdjacentBlock->Tag == TZoneTag::Free)
	{
		pAdjacentBlock->nSize += pBlock->nSize;
		pAdjacentBlock->pNext            = pBlock->pNext;
		pAdjacentBlock->pNext->pPrevious = pAdjacentBlock;
		// Next allocations search from this new merged free block
		if (pBlock == m_pCurrentBlock)
			m_pCurrentBlock = pAdjacentBlock;
#ifdef ZONE_ALLOCATOR_TRACE
		CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Merged freed block at %p with previous block at %p", pPtr, pAdjacentBlock);
#endif
		pBlock = pAdjacentBlock;
	}

	// Join with next block if next block is also free
	pAdjacentBlock = pBlock->pNext;
	if (pAdjacentBlock->Tag == TZoneTag::Free)
	{
		pBlock->nSize += pAdjacentBlock->nSize;
		pBlock->pNext            = pAdjacentBlock->pNext;
		pBlock->pNext->pPrevious = pBlock;
		if (pAdjacentBlock == m_pCurrentBlock)
			m_pCurrentBlock = pBlock;
#ifdef ZONE_ALLOCATOR_TRACE
		CLogger::Get()->Write(ZoneAllocatorName, LogDebug, "Merged freed block at %p with next block at %p", pPtr, pAdjacentBlock);
#endif
	}

	// Decrement allocation counter
	--m_nAllocCount;
}

void CZoneAllocator::Clear()
{
	TBlock* pFirstBlock = static_cast<TBlock*>(m_pHeap);

	// The main block is a special block which acts as an end marker for the linked list of blocks
	m_MainBlock.nSize     = 0;
	m_MainBlock.pNext     = pFirstBlock;
	m_MainBlock.pPrevious = pFirstBlock;
	m_MainBlock.Tag       = TZoneTag::Uncategorized;
	m_MainBlock.nMagic    = 0;
#if AARCH == 32
	// 0xEB - "extra byte"; useful for memory view when debugging
	memset(m_MainBlock.Padding, 0xEB, Utility::ArraySize(m_MainBlock.Padding));
#endif

	pFirstBlock->nSize     = m_nHeapSize;
	pFirstBlock->pNext     = &m_MainBlock;
	pFirstBlock->pPrevious = &m_MainBlock;
	pFirstBlock->Tag       = TZoneTag::Free;
	pFirstBlock->nMagic    = BlockMagic;
#if AARCH == 32
	memset(pFirstBlock->Padding, 0xEB, Utility::ArraySize(pFirstBlock->Padding));
#endif

	m_pCurrentBlock = pFirstBlock;
}

void CZoneAllocator::FreeTag(u32 Tag)
{
	if (Tag == TZoneTag::Free)
	{
		CLogger::Get()->Write(ZoneAllocatorName, LogError, "Attempted to free an invalid tag");
		return;
	}

	TBlock* pBlock = m_MainBlock.pNext;
	TBlock* pNextBlock;

	do
	{
		// Grab the next block before freeing this one
		pNextBlock = pBlock->pNext;
		if (pBlock->Tag == Tag)
			Free(reinterpret_cast<u8*>(pBlock) + sizeof(TBlock));
		pBlock = pNextBlock;
	} while (pBlock != &m_MainBlock);
}

void CZoneAllocator::Dump() const
{
	CLogger* pLogger = CLogger::Get();
	pLogger->Write(ZoneAllocatorName, LogNotice, "Allocation diagnostics:");

	TBlock* pBlock = m_MainBlock.pNext;

	do
	{
		pLogger->Write(ZoneAllocatorName, LogNotice, "Block address %p (%s):", pBlock, pBlock->Tag ? "IN-USE" : "FREE");

		// If the block is free, it doesn't need a valid tail magic
		const bool bMagicOK = (pBlock->nMagic == BlockMagic) && (!pBlock->Tag || GetEndMagic(pBlock) == BlockMagic);
		if (!bMagicOK)
			pLogger->Write(ZoneAllocatorName, LogWarning, "WARNING: This memory block is probably corrupt!");

		pLogger->Write(ZoneAllocatorName, LogNotice, "\tSize:  %d bytes", pBlock->nSize);
		pLogger->Write(ZoneAllocatorName, LogNotice, "\tTag:   0x%x", pBlock->Tag);
		pLogger->Write(ZoneAllocatorName, LogNotice, "\tMagic: %s", bMagicOK ? "OK" : "BAD");
		pBlock = pBlock->pNext;
	} while (pBlock != &m_MainBlock);
}
