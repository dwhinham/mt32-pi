//
// utility.h
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

#ifndef _utility_h
#define _utility_h

#include <circle/string.h>
#include <circle/util.h>

// Macro to extract the string representation of an enum
#define CONFIG_ENUM_VALUE(VALUE, STRING) VALUE,

// Macro to extract the enum value
#define CONFIG_ENUM_STRING(VALUE, STRING) #STRING,

// Macro to declare the enum itself
#define CONFIG_ENUM(NAME, VALUES) enum class NAME { VALUES(CONFIG_ENUM_VALUE) }

// Macro to declare an array of string representations for an enum
#define CONFIG_ENUM_STRINGS(NAME, DATA) static const char* NAME##Strings[] = { DATA(CONFIG_ENUM_STRING) }

namespace Utility
{
	// Templated function for clamping a value between a minimum and a maximum
	template <class T>
	constexpr T Clamp(T value, T min, T max)
	{
		return (value < min) ? min : (value > max) ? max : value;
	}

	// Templated function for taking the minimum of two values
	template <class T>
	constexpr T Min(T lhs, T rhs)
	{
		return lhs < rhs ? lhs : rhs;
	}

	// Templated function for taking the maximum of two values
	template <class T>
	constexpr T Max(T lhs, T rhs)
	{
		return lhs > rhs ? lhs : rhs;
	}

	// Return number of elements in an array
	template <class T, size_t N>
	constexpr size_t ArraySize(const T(&)[N]) { return N; }

	// Returns whether some value is a power of 2
	template <class T>
	constexpr bool IsPowerOfTwo(T value)
	{
		return value && ((value & (value-1)) == 0);
	}

	// Rounds a number to a nearest multiple; only works for integer values/multiples
	template <class T>
	constexpr T RoundToNearestMultiple(T nValue, T nMultiple)
	{
		return ((nValue + nMultiple / 2) / nMultiple) * nMultiple;
	}

	// Computes the Roland checksum
	constexpr u8 RolandChecksum(const u8* pData, size_t nSize)
	{
		u8 nSum = 0;
		for (size_t i = 0; i < nSize; ++i)
			nSum = (nSum + pData[i]) & 0x7F;

		return 128 - nSum;
	}

	// Comparators for sorting
	namespace Comparator
	{
		template<class T>
		using TComparator = bool (*)(const T&, const T&);

		template<class T>
		inline bool LessThan(const T& lhs, const T& rhs)
		{
			return lhs < rhs;
		}

		template<class T>
		inline bool GreaterThan(const T& ItemA, const T& ItemB)
		{
			return ItemA > ItemB;
		}

		inline bool CaseInsensitiveAscending(const CString& lhs, const CString& rhs)
		{
			return strcasecmp(lhs, rhs) < 0;
		}
	}

	// Swaps two objects in-place
	template<class T>
	inline void Swap(T& lhs, T& rhs)
	{
		u8 temp[sizeof(T)];
		memcpy(temp, &lhs, sizeof(T));
		memcpy(&lhs, &rhs, sizeof(T));
		memcpy(&rhs, temp, sizeof(T));
	}

	namespace
	{
		// Quicksort partition function (private)
		template<class T, size_t N>
		size_t Partition(T(&Items)[N], Comparator::TComparator<T> Comparator, size_t nLow, size_t nHigh)
		{
			const size_t nPivotIndex = (nHigh + nLow) / 2;
			T* Pivot = &Items[nPivotIndex];

			while (true)
			{
				while (Comparator(Items[nLow], *Pivot))
					++nLow;

				while (Comparator(*Pivot, Items[nHigh]))
					--nHigh;

				if (nLow >= nHigh)
					return nHigh;

				Swap(Items[nLow], Items[nHigh]);

				// Update pointer if pivot was swapped
				if (nPivotIndex == nLow)
					Pivot = &Items[nHigh];
				else if (nPivotIndex == nHigh)
					Pivot = &Items[nLow];

				++nLow;
				--nHigh;
			}
		}
	}

	// Sorts an array in-place using the Tony Hoare Quicksort algorithm
	template <class T, size_t N>
	void QSort(T(&Items)[N], Comparator::TComparator<T> Comparator = Comparator::LessThan<T>, size_t nLow = 0, size_t nHigh = N - 1)
	{
		if (nLow < nHigh)
		{
			size_t p = Partition(Items, Comparator, nLow, nHigh);
			QSort(Items, Comparator, nLow, p);
			QSort(Items, Comparator, p + 1, nHigh);
		}
	}
}

#endif
