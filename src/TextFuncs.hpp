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

/** Text conversion functions **/

#ifndef CP_ACP
# define CP_ACP 0
#endif
#ifndef CP_LATIN1
# define CP_LATIN1 28591
#endif
#ifndef CP_UTF8
# define CP_UTF8 65001
#endif

// Text conversion flags.
typedef enum {
	// Enable cp1252 fallback if the text fails to
	// decode using the specified code page.
	TEXTCONV_FLAG_CP1252_FALLBACK		= (1 << 0),
} TextConv_Flags_e;

/**
 * Convert 8-bit text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 *
 * The specified code page number will be used.
 *
 * @param cp	[in] Code page number.
 * @param str	[in] 8-bit text.
 * @param len	[in] Length of str, in bytes. (-1 for NULL-terminated string)
 * @param flags	[in] Flags. (See TextConv_Flags_e.)
 * @return UTF-8 string.
 */
std::string cpN_to_utf8(unsigned int cp, const char *str, int len, unsigned int flags = 0);

/**
 * Convert 8-bit text to UTF-16.
 * WARNING: This function does NOT support NULL-terminated strings!
 *
 * The specified code page number will be used.
 *
 * @param cp	[in] Code page number.
 * @param str	[in] 8-bit text.
 * @param len	[in] Length of str, in bytes.
 * @param flags	[in] Flags. (See TextConv_Flags_e.)
 * @return UTF-16 string.
 */
std::u16string cpN_to_utf16(unsigned int cp, const char *str, int len, unsigned int flags = 0);

/**
 * Convert UTF-8 to 8-bit text.
 * WARNING: This function does NOT support NULL-terminated strings!
 *
 * The specified code page number will be used.
 * Invalid characters will be ignored.
 *
 * @param cp	[in] Code page number.
 * @param str	[in] UTF-8 text.
 * @param len	[in] Length of str, in bytes.
 * @return 8-bit text.
 */
std::string utf8_to_cpN(unsigned int cp, const char *str, int len);

/* UTF-8 to UTF-16 and vice-versa */

/**
 * Convert UTF-8 text to UTF-16.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param str	[in] UTF-8 text.
 * @param len	[in] Length of str, in bytes.
 * @return UTF-16 string.
 */
static inline std::u16string utf8_to_utf16(const char *str, size_t len)
{
	return cpN_to_utf16(CP_UTF8, str, static_cast<int>(len));
}

/**
 * Convert UTF-8 text to UTF-16.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param str UTF-8 string.
 * @return UTF-16 string.
 */
static inline std::u16string utf8_to_utf16(const std::string &str)
{
	return cpN_to_utf16(CP_UTF8, str.data(), static_cast<int>(str.size()));
}

/* Specialized UTF-16 conversion functions */

/**
 * Convert UTF-16LE text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs	[in] UTF-16LE text.
 * @param len	[in] Length of wcs, in characters.
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

/* UTF-16 to UTF-16 conversion functions */

/**
 * Byteswap and return UTF-16 text.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs UTF-16 text to byteswap.
 * @param len Length of wcs, in characters.
 * @return Byteswapped UTF-16 string.
 */
std::u16string utf16_bswap(const char16_t *wcs, size_t len);

/**
 * Convert UTF-16LE text to host-endian UTF-16.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs UTF-16LE text.
 * @param len Length of wcs, in characters.
 * @return Host-endian UTF-16 string.
 */
static inline std::u16string utf16le_to_utf16(const char16_t *wcs, int len)
{
#if SYS_BYTEORDER == SYS_LIL_ENDIAN
	return std::u16string(wcs, len);
#else /* SYS_BYTEORDER == SYS_BIG_ENDIAN */
	return utf16_bswap(wcs, len);
#endif
}

/**
 * Convert UTF-16BE text to host-endian UTF-16.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs UTF-16BLE text.
 * @param len Length of wcs, in characters.
 * @return Host-endian UTF-16 string.
 */
static inline std::u16string utf16be_to_utf16(const char16_t *wcs, int len)
{
#if SYS_BYTEORDER == SYS_LIL_ENDIAN
	return utf16_bswap(wcs, len);
#else /* SYS_BYTEORDER == SYS_BIG_ENDIAN */
	return std::u16string(wcs, len);
#endif
}

#endif /* __MST06_TEXTFUNCS_HPP__ */
