/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * common.h: Common types and macros.                                      *
 *                                                                         *
 * Copyright (c) 2016-2025 by David Korth.                                 *
 * SPDX-License-Identifier: MIT                                            *
 ***************************************************************************/

#pragma once

#ifdef __cplusplus
#  include <cstddef>
#else
#  include <stddef.h>
#endif

/**
 * Number of elements in an array.
 *
 * Includes a static check for pointers to make sure
 * a dynamically-allocated array wasn't specified.
 * Reference: http://stackoverflow.com/questions/8018843/macro-definition-array-size
 */
#define ARRAY_SIZE(x) \
	((int)(((sizeof(x) / sizeof(x[0]))) / \
		(size_t)(!(sizeof(x) % sizeof(x[0])))))

// PACKED struct attribute.
// Use in conjunction with #pragma pack(1).
#ifdef __GNUC__
#  define PACKED __attribute__((packed))
#else
#  define PACKED
#endif

/**
 * static_asserts size of a structure
 * Also defines a constant of form StructName_SIZE
 */
// TODO: Check MSVC support for static_assert() in C mode.
#if defined(__cplusplus)
#  define ASSERT_STRUCT(st,sz) enum { st##_SIZE = (sz), }; \
	static_assert(sizeof(st)==(sz),#st " is not " #sz " bytes.")
#elif defined(__GNUC__) && defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#  define ASSERT_STRUCT(st,sz) enum { st##_SIZE = (sz), }; \
	_Static_assert(sizeof(st)==(sz),#st " is not " #sz " bytes.")
#else
#  define ASSERT_STRUCT(st, sz)
#endif

// Deprecated function attribute.
#ifndef DEPRECATED
#  if defined(__GNUC__)
#    define DEPRECATED __attribute__ ((deprecated))
#  elif defined(_MSC_VER)
#    define DEPRECATED __declspec(deprecated)
#  else
#    define DEPRECATED
#  endif
#endif

// Force inline attribute.
#if !defined(FORCEINLINE)
#  if (!defined(_DEBUG) || defined(NDEBUG))
#    if defined(__GNUC__)
#      define FORCEINLINE inline __attribute__((always_inline))
#    elif defined(_MSC_VER)
#      define FORCEINLINE __forceinline
#    else
#      define FORCEINLINE inline
#    endif
#  else
#    ifdef _MSC_VER
#      define FORCEINLINE __inline
#    else
#      define FORCEINLINE inline
#    endif
#  endif
#endif /* !defined(FORCEINLINE) */

// gcc branch prediction hints.
// Should be used in combination with profile-guided optimization.
#ifdef __GNUC__
#  define likely(x)	__builtin_expect(!!(x), 1)
#  define unlikely(x)	__builtin_expect(!!(x), 0)
#else
#  define likely(x)	x
#  define unlikely(x)	x
#endif

// C99 restrict macro.
// NOTE: gcc only defines restrict in C, not C++,
// so use __restrict on both gcc and MSVC.
#define RESTRICT __restrict

/**
 * Alignment macro.
 * @param a	Alignment value.
 * @param x	Byte count to align.
 */
// FIXME: No __typeof__ in MSVC's C mode...
#if defined(_MSC_VER) && !defined(__cplusplus)
#  define ALIGN(a, x)	(((x)+((a)-1)) & ~((uint64_t)((a)-1)))
#else
#  define ALIGN(a, x)	(((x)+((a)-1)) & ~((__typeof__(x))((a)-1)))
#endif

/**
 * Alignment assertion macro.
 */
#define ASSERT_ALIGNMENT(a, ptr)	assert(reinterpret_cast<uintptr_t>(ptr) % (a) == 0);

/**
 * Aligned variable macro.
 * @param a Alignment value.
 * @param decl Variable declaration.
 */
#if defined(__GNUC__)
#  define ALIGNED_VAR(a, decl)	decl __attribute__((aligned(a)))
#elif defined(_MSC_VER)
#  define ALIGNED_VAR(a, decl)	__declspec(align(a)) decl
#else
#  error No aligned variable macro for this compiler.
#endif
