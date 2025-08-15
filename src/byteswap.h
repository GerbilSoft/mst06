/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * byteswap.h: Byteswapping functions.                                     *
 *                                                                         *
 * Copyright (c) 2008-2025 by David Korth.                                 *
 * SPDX-License-Identifier: MIT                                            *
 ***************************************************************************/

#pragma once

// C includes
#include <stdint.h>

/* Byteswapping intrinsics */
#include "config.byteswap.h"

/* Get the system byte order */
#include "byteorder.h"

#if defined(_MSC_VER)

/* Use the MSVC byteswap intrinsics. */
#include <stdlib.h>
#define __swab16(x) _byteswap_ushort(x)
#define __swab32(x) _byteswap_ulong(x)
#define __swab64(x) _byteswap_uint64(x)

/* `inline` might not be defined in older versions. */
#ifndef inline
# define inline __inline
#endif

#else /* !defined(_MSC_VER) */

/* Use gcc's byteswap intrinsics if available. */

#ifdef HAVE___BUILTIN_BSWAP16
#define __swab16(x) ((uint16_t)__builtin_bswap16(x))
#else
#define __swab16(x) ((uint16_t)(((x) << 8) | ((x) >> 8)))
#endif

#ifdef HAVE___BUILTIN_BSWAP32
#define __swab32(x) ((uint32_t)__builtin_bswap32(x))
#else
#define __swab32(x) \
	((uint32_t)(((x) << 24) | ((x) >> 24) | \
		(((x) & 0x0000FF00UL) << 8) | \
		(((x) & 0x00FF0000UL) >> 8)))
#endif

#ifdef HAVE___BUILTIN_BSWAP64
#define __swab64(x) ((uint64_t)__builtin_bswap64(x))
#else
#define __swab64(x) \
	((uint64_t)(((x) << 56) | ((x) >> 56) | \
		(((x) & 0x000000000000FF00ULL) << 40) | \
		(((x) & 0x0000000000FF0000ULL) << 24) | \
		(((x) & 0x00000000FF000000ULL) << 8) | \
		(((x) & 0x000000FF00000000ULL) >> 8) | \
		(((x) & 0x0000FF0000000000ULL) >> 24) | \
		(((x) & 0x00FF000000000000ULL) >> 40)))
#endif

#endif /* defined(_MSC_VER) */

#if SYS_BYTEORDER == SYS_LIL_ENDIAN
	#define be16_to_cpu(x)	__swab16(x)
	#define be32_to_cpu(x)	__swab32(x)
	#define be64_to_cpu(x)	__swab64(x)
	#define le16_to_cpu(x)	(x)
	#define le32_to_cpu(x)	(x)
	#define le64_to_cpu(x)	(x)

	#define cpu_to_be16(x)	__swab16(x)
	#define cpu_to_be32(x)	__swab32(x)
	#define cpu_to_be64(x)	__swab64(x)
	#define cpu_to_le16(x)	(x)
	#define cpu_to_le32(x)	(x)
	#define cpu_to_le64(x)	(x)
#else /* SYS_BYTEORDER == SYS_BIG_ENDIAN */
	#define be16_to_cpu(x)	(x)
	#define be32_to_cpu(x)	(x)
	#define be64_to_cpu(x)	(x)
	#define le16_to_cpu(x)	__swab16(x)
	#define le32_to_cpu(x)	__swab32(x)
	#define le64_to_cpu(x)	__swab64(x)

	#define cpu_to_be16(x)	(x)
	#define cpu_to_be32(x)	(x)
	#define cpu_to_be64(x)	(x)
	#define cpu_to_le16(x)	__swab16(x)
	#define cpu_to_le32(x)	__swab32(x)
	#define cpu_to_le64(x)	__swab64(x)
#endif
