//
// barchars.h
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

#ifndef _barchars_h
#define _barchars_h

#include <circle/types.h>

// Custom characters for drawing bar graphs
// Empty block character already exists in the character set (ASCII space, 0x20)
constexpr u8 CustomBarCharDataWide[][8] =
{
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b00000,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	},
	{
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111,
		0b11111
	}
};

constexpr u8 CustomBarCharDataNarrow[][8] =
{
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b01110
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b01110,
		0b01110
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b01110,
		0b01110,
		0b01110
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b01110,
		0b01110,
		0b01110,
		0b01110
	},
	{
		0b00000,
		0b00000,
		0b00000,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110
	},
	{
		0b00000,
		0b00000,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110
	},
	{
		0b00000,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110
	},
	{
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110,
		0b01110
	}
};

// Use ASCII space for empty bar, custom chars for 1-8 rows
// N.B. Use indices 0x08-0x0F into CG RAM instead of 0x00-0x07 to allow us to use them in C strings (avoid clash with null terminator)
constexpr char BarChars[] = { ' ', '\x08', '\x09', '\x0A', '\x0B', '\x0C', '\x0D', '\x0E', '\x0F' };

#endif
