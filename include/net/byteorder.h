//
// byteorder.h
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

#ifndef _byteorder_h
#define _byteorder_h

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define htons(VALUE)  (VALUE)
#define htonl(VALUE)  (VALUE)
#define htonll(VALUE) (VALUE)
#define ntohs(VALUE)  (VALUE)
#define ntohl(VALUE)  (VALUE)
#define ntohll(VALUE) (VALUE)
#else
#define htons(VALUE)  __builtin_bswap16(VALUE)
#define htonl(VALUE)  __builtin_bswap32(VALUE)
#define htonll(VALUE) __builtin_bswap64(VALUE)
#define ntohs(VALUE)  __builtin_bswap16(VALUE)
#define ntohl(VALUE)  __builtin_bswap32(VALUE)
#define ntohll(VALUE) __builtin_bswap64(VALUE)
#endif

#endif
