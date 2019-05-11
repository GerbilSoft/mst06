/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * mst_structs.h: MST/BINA data structures.                                *
 *                                                                         *
 * Copyright (c) 2019 by David Korth.                                      *
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

#ifndef __MST06_MST_STRUCTS_H__
#define __MST06_MST_STRUCTS_H__

#ifndef PACKED
# ifdef __GNUC__
#  define PACKED __attribute__((packed))
# else
#  define PACKED
# endif
#endif /* !PACKED */

#include <stdint.h>

#pragma pack(1)

/**
 * MST file header.
 * Currently, only Sonic'06 files are supported.
 *
 * All offsets are relative to the end of the header.
 *
 * Field endianness is determined by the 'endianness' field.
 */
#define BINA_MAGIC 'BINA'
typedef struct PACKED _MST_Header {
	uint32_t file_size;		// [0x000] Total size of the MST file.
	uint32_t doff_tbl_offset;	// [0x004] Start of the differential offset table.
	uint32_t doff_tbl_length;	// [0x008] Differential offset table length.
	uint32_t unk_zero1;		// [0x00C]
	uint32_t unk_zero2;		// [0x010]
	uint16_t unk_zero3;		// [0x014]
	char version;			// [0x016] Version. ('1')
	char endianness;		// [0x017] 'B' for big-endian; 'L' for little-endian.
	uint32_t bina_magic;		// [0x018] 'BINA'
	uint32_t unk_zero4;		// [0x01C]
} MST_Header;

/**
 * WTXT header.
 * Indicates the number of messages in the file.
 *
 * Field endianness is determined by the 'endianness' field
 * in the MST header.
 */
#define WTXT_MAGIC 'WTXT'
typedef struct PACKED _WTXT_Header {
	uint32_t magic;			// [0x000] 'WTXT'
	uint32_t msg_tbl_name_offset;	// [0x004] Offset of message table name.
	uint32_t msg_tbl_count;		// [0x008] Number of strings in the message table.
					//         NOTE: May be incorrect if empty strings
					//         are present... Maybe this should simply
					//         be "number of offsets / 3"?
} WTXT_Header;

/**
 * Following WTXT_Header is an array of message pointers.
 * Messages are encded as UTF-16BE.
 *
 * NOTE: The offset table should be used to determine the actual offsets.
 */
typedef struct PACKED _WTXT_MsgPointer {
	uint32_t msg_id_name_offset;	// [0x000] Offset of message name.
	uint32_t msg_offset;		// [0x004] Offset of message.
	uint32_t zero;			// [0x008] Zero. (NOTE: May not be present!)
} WTXT_MsgPointer;

#pragma pack()

#endif /* __MST06_MST_STRUCTS_H__ */
