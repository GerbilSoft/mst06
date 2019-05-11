/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * TextFuncs.hpp: Text encoding functions.                                 *
 *                                                                         *
 * Copyright (c) 2009-2019 by David Korth.                                 *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 3 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 ***************************************************************************/

#ifndef __MST06_TEXTFUNCS_HPP__
#define __MST06_TEXTFUNCS_HPP__

#include "byteorder.h"

// C++ includes.
#include <string>

/**
 * Convert UTF-16LE text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs	[in] UTF-16LE text.
 * @param len	[in] Length of wcs, in characters. (-1 for NULL-terminated string)
 * @return UTF-8 string.
 */
std::string utf16le_to_utf8(const char16_t *wcs, size_t len);

/**
 * Convert UTF-16LE text to UTF-8.
 * @param wcs	[in] UTF-16LE text.
 * @return UTF-8 text.
 */
inline std::string utf16le_to_utf8(const std::u16string &wcs)
{
	return utf16le_to_utf8(wcs.data(), wcs.size());
}

/**
 * Convert UTF-16BE text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs	[in] UTF-16BE text.
 * @param len	[in] Length of wcs, in characters.
 * @return UTF-8 string.
 */
std::string utf16be_to_utf8(const char16_t *wcs, size_t len);

/**
 * Convert UTF-16BE text to UTF-8.
 * @param wcs	[in] UTF-16BE text.
 * @return UTF-8 text.
 */
inline std::string utf16be_to_utf8(const std::u16string &wcs)
{
	return utf16be_to_utf8(wcs.data(), wcs.size());
}

/**
 * Convert UTF-16 host-endian text to UTF-8.
 * @param wcs	[in] UTF-16 host-endian text.
 * @return UTF-8 text.
 */
static inline std::string utf16_to_utf8(const std::u16string &wcs)
{
#if SYS_BYTEORDER == SYS_LIL_ENDIAN
	return utf16le_to_utf8(wcs);
#else /* SYS_BYTEORDER == SYS_BIG_ENDIAN */
	return utf16be_to_utf8(wcs);
#endif
}

#endif /* __MST06_TEXTFUNCS_HPP__ */
