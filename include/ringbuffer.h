//
// ringbuffer.h
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

#ifndef _ringbuffer_h
#define _ringbuffer_h

#include <circle/spinlock.h>
#include <circle/types.h>

#include "utility.h"

template <class T, size_t N>
class CRingBuffer
{
public:
	CRingBuffer()
		: m_Lock(IRQ_LEVEL),
		  m_nInPtr(0),
		  m_nOutPtr(0),
		  m_Data{}
	{
	}

	bool Enqueue(const T& Item)
	{
		bool bSuccess;
		m_Lock.Acquire();

		bSuccess = EnqueueOne(Item);

		m_Lock.Release();
		return bSuccess;
	}

	size_t Enqueue(const T* pItems, size_t nCount)
	{
		size_t nEnqueued = 0;
		m_Lock.Acquire();

		for (size_t i = 0; i < nCount; ++i)
		{
			if (EnqueueOne(pItems[i]))
				++nEnqueued;
		}

		m_Lock.Release();
		return nEnqueued;
	}

	bool Dequeue(T& OutItem)
	{
		bool bSuccess = false;
		m_Lock.Acquire();

		if (m_nInPtr != m_nOutPtr)
		{
			OutItem = m_Data[m_nOutPtr++];
			m_nOutPtr &= BufferMask;
			bSuccess = true;
		}

		m_Lock.Release();
		return bSuccess;
	}

	size_t Dequeue(T* pOutBuffer, size_t nMaxCount)
	{
		size_t nDequeued = 0;
		m_Lock.Acquire();

		while (m_nInPtr != m_nOutPtr)
		{
			pOutBuffer[nDequeued++] = m_Data[m_nOutPtr++];
			m_nOutPtr &= BufferMask;
		}

		m_Lock.Release();
		return nDequeued;
	}

private:
	static_assert(Utility::IsPowerOfTwo(N), "Ring buffer size must be a power of 2");

	inline bool EnqueueOne(const T& Item)
	{
		if (((m_nInPtr + 1) & BufferMask) != m_nOutPtr)
		{
			m_Data[m_nInPtr++] = Item;
			m_nInPtr &= BufferMask;
			return true;
		}

		return false;
	}

	static constexpr size_t BufferMask = N - 1;

	CSpinLock m_Lock;
	volatile size_t m_nInPtr;
	volatile size_t m_nOutPtr;
	T m_Data[N];
};

#endif
