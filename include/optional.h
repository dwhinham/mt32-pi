//
// optional.h
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

#ifndef _optional_h
#define _optional_h

#include <assert.h>
#include <circle/new.h>
#include <circle/types.h>

template<typename T>
class TOptional
{
public:
	TOptional()
		: m_bSet(false)
	{
	}

	TOptional(const TOptional<T>& Other)
	{
		operator=(Other);
	}

	template<typename U>
	TOptional(U&& Value)
	{
		m_bSet = true;
		new(reinterpret_cast<T*>(m_Value)) T(static_cast<U&&>(Value));
	}

	~TOptional() { Reset(); }

	void Reset()
	{
		if (m_bSet)
		{
			m_bSet = false;
			reinterpret_cast<T*>(m_Value)->T::~T();
		}
	}

	constexpr const T& Value() const
	{
		assert(m_bSet);
		return *reinterpret_cast<const T*>(m_Value);
	}

	constexpr const T& ValueOr(const T& OtherValue) const
	{
		return m_bSet ? Value() : OtherValue;
	}

	constexpr explicit operator bool() const
	{
		return m_bSet;
	}

	TOptional<T>& operator =(const TOptional<T>& Other)
	{
		m_bSet = true;
		*reinterpret_cast<T*>(m_Value) = *reinterpret_cast<const T*>(Other.m_Value);
		return *this;
	}

	constexpr const T& operator *() const { return Value();	}
	constexpr const T* operator ->() const { return &Value(); }
	constexpr const T* operator &() const {	return &Value(); }

private:
	bool m_bSet;
	alignas(T) u8 m_Value[sizeof(T)];
};

#endif
