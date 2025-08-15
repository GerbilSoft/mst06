/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * tcharx.h: TCHAR support for Windows and Linux.                          *
 *                                                                         *
 * Copyright (c) 2018-2025 by David Korth.                                 *
 * SPDX-License-Identifier: MIT                                            *
 ***************************************************************************/

#ifndef __MST06_TCHARX_H__
#define __MST06_TCHARX_H__

#ifdef _WIN32

// Windows: Use the SDK tchar.h.
#include <tchar.h>

// NOTE: tcharx.h has _sntprintf(), but not sntprintf().
// As of MSVC 2015, snprintf() always NULL-terminates the buffer,
// but _snprintf(), and thus _sntprintf(), does *not* in some cases.
// We're switching to our own sntprintf() macro to handle this.
// Reference: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/snprintf-snprintf-snprintf-l-snwprintf-snwprintf-l?view=msvc-170
#ifdef _sntprintf
#  undef _sntprintf
#endif
#define _snprintf __DO_NOT_USE_OLD_MSVC__SNPRINTF
#define _sntprintf __DO_NOT_USE_OLD_MSVC__SNTPRINTF
#define _snwprintf __DO_NOT_USE_OLD_MSVC__SNWPRINTF
#define _swprintf __DO_NOT_USE_OLD_MSVC__SWPRINTF

// std::tstring
#ifdef _UNICODE
#  define sntprintf swprintf
#  define tstring wstring
#else /* !_UNICODE */
#  define sntprintf snprintf
#  define tstring string
#endif /* _UNICODE */

#else /* !_WIN32 */

// Other systems: Define TCHAR and related macros.
typedef char TCHAR;
#define _T(x) x
#define _tmain main
#define sntprintf snprintf
#define tstring string

// ctype.h
#define _istalpha(c) isalpha(c)

// stdio.h
#define _fputts(s, stream) fputs(s, stream)

#define _tfopen(filename, mode) fopen((filename), (mode))

#define _puttc putc
#define _fputtc fputc
#define _puttchar putchar
#define _fputtchar fputchar

#define _tprintf printf
#define _ftprintf fprintf
#define _vtprintf vprintf
#define _vftprintf vfprintf

// stdlib.h
#define _tcscmp(s1, s2)			strcmp((s1), (s2))
#define _tcsicmp(s1, s2)		strcasecmp((s1), (s2))
#define _tcsnicmp(s1, s2)		strncasecmp((s1), (s2), (n))
#define _tcstoul(nptr, endptr, base)	strtoul((nptr), (endptr), (base))

// string.h
#define _tcsdup(s) strdup(s)
#define _tcserror(err) strerror(err)

#endif /* _WIN32 */

#endif /* __MST06_TCHARX_H__ */
