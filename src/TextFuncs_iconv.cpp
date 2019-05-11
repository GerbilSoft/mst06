/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * TextFuncs.cpp: Text encoding functions.                                 *
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

#include "TextFuncs.hpp"

// C++ includes.
#include <string>
using std::u16string;
using std::string;

// iconv
#include <iconv.h>

/** OS-specific text conversion functions. **/

/**
 * Convert a string from one character set to another.
 * @param src 		[in] Source string.
 * @param len           [in] Source length, in bytes.
 * @param src_charset	[in] Source character set.
 * @param dest_charset	[in] Destination character set.
 * @return malloc()'d UTF-8 string, or nullptr on error.
 */
static char *rp_iconv(const char *src, int len,
		const char *src_charset, const char *dest_charset)
{
	if (!src || len <= 0)
		return nullptr;

	if (!src_charset)
		src_charset = "";
	if (!dest_charset)
		dest_charset = "";

	// Based on examples from:
	// * http://www.delorie.com/gnu/docs/glibc/libc_101.html
	// * http://www.codase.com/search/call?name=iconv

	// Open an iconv descriptor.
	iconv_t cd;
	cd = iconv_open(dest_charset, src_charset);
	if (cd == (iconv_t)(-1)) {
		// Error opening iconv.
		return nullptr;
	}

	// Allocate the output buffer.
	// UTF-8 is variable length, and the largest UTF-8 character is 4 bytes long.
	size_t src_bytes_len = (size_t)len;
	const size_t out_bytes_len = (src_bytes_len * 4) + 4;
	size_t out_bytes_remaining = out_bytes_len;
	char *outbuf = static_cast<char*>(malloc(out_bytes_len));

	// Input and output pointers.
	char *inptr = const_cast<char*>(src);	// Input pointer.
	char *outptr = &outbuf[0];		// Output pointer.

	bool success = true;
	while (src_bytes_len > 0) {
		if (iconv(cd, &inptr, &src_bytes_len, &outptr, &out_bytes_remaining) == (size_t)(-1)) {
			// An error occurred while converting the string.
			// FIXME: Flag to indicate that we want to have
			// a partial Shift-JIS conversion?
			// Madou Monogatari I (MD) has a broken Shift-JIS
			// code point, which breaks conversion.
			// (Reported by andlabs.)
			success = false;
			break;
		}
	}

	// Close the iconv descriptor.
	iconv_close(cd);

	if (success) {
		// The string was converted successfully.

		// Make sure the string is null-terminated.
		size_t null_bytes = (out_bytes_remaining > 4 ? 4 : out_bytes_remaining);
		for (size_t i = null_bytes; i > 0; i--) {
			*outptr++ = 0x00;
		}

		// Return the output buffer.
		return outbuf;
	}

	// The string was not converted successfully.
	free(outbuf);
	return nullptr;
}

/** Unicode to Unicode conversion functions. **/

/**
 * Convert UTF-16LE text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs	[in] UTF-16LE text.
 * @param len	[in] Length of wcs, in characters.
 * @return UTF-8 string.
 */
std::string utf16le_to_utf8(const char16_t *wcs, size_t len)
{
	// Attempt to convert the text from UTF-16LE to UTF-8.
	string ret;
	char *mbs = reinterpret_cast<char*>(rp_iconv((char*)wcs, len*sizeof(char16_t), "UTF-16LE", "UTF-8"));
	if (mbs) {
		ret.assign(mbs);
		free(mbs);
	}
	return ret;
}

/**
 * Convert UTF-16BE text to UTF-8.
 * WARNING: This function does NOT support NULL-terminated strings!
 * @param wcs	[in] UTF-16BE text.
 * @param len	[in] Length of wcs, in characters.
 * @return UTF-8 string.
 */
std::string utf16be_to_utf8(const char16_t *wcs, size_t len)
{
	// Attempt to convert the text from UTF-16BE to UTF-8.
	string ret;
	char *mbs = reinterpret_cast<char*>(rp_iconv((char*)wcs, len*sizeof(char16_t), "UTF-16BE", "UTF-8"));
	if (mbs) {
		ret.assign(mbs);
		free(mbs);
	}
	return ret;
}
