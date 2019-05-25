/***************************************************************************
 * ROM Properties Page shell extension. (librpbase)                        *
 * TextFuncs.cpp: Text encoding functions.                                 *
 *                                                                         *
 * Copyright (c) 2009-2019 by David Korth.                                 *
 * SPDX-License-Identifier: GPL-3.0-or-later                               *
 ***************************************************************************/

#include "TextFuncs.hpp"
#include "byteswap.h"

// C++ includes.
#include <string>
using std::u16string;

/**
 * Byteswap and return UTF-16 text.
 * @param str UTF-16 text to byteswap.
 * @param len Length of str, in characters.
 * @return Byteswapped UTF-16 string.
 */
u16string utf16_bswap(const char16_t *str, size_t len)
{
	if (len == 0) {
		return u16string();
	}

	// TODO: Optimize this?
	u16string ret;
	ret.reserve(len);
	for (; len > 0; len--, str++) {
		ret += __swab16(*str);
	}

	return ret;
}
