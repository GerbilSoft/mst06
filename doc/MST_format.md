# Sonic '06 MST Format

This document is Copyright (C) 2019 by David Korth.<br>
Licensed under the GNU Free Documentation License v1.3.

## What is MST?

The MST ("Message Table") format is used by Sonic '06 to store all
in-game messages. It has a few quirks that are unique to the MST and
BINA formats.

**NOTE:** All offsets, including those in the header, are relative to
*after* the end of the header. The header is 32 bytes, so add 32 to
all offsets to get the absolute address.

## File Order

* Header
* WTXT Header and offset table
* All message text (encoded as UTF-16)
* All message names (encoded as Shift-JIS)
* Differential offset table

The string table name is included in the "all message names" block.

The differential offset table must be DWORD-aligned, both for its starting
offset and for its total size. The message text block is usually DWORD-aligned,
since the offset table consists of 32-bit units. The message names block does
not have to be DWORD-aligned, though it is always WORD-aligned, since message
text is encoded as UTF-16.

## Header

```c
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
```

Fields:
* `file_size`: Total file size.
* `offset_tbl_offset`: Start of the differential offset table.
* `offset_tbl_length`: Length of the differential offset table.
* `version`: MST version. ('1')
* `endianness`: File endianness. ('B' for big-endian; 'L' for little-endian.)
* `bina_magic`: "BINA".

Note that for Sonic '06, version is always '1', and endianness is always 'B'.

## Offset Table

The offset table starts with a WTXT header:

```c
#define WTXT_MAGIC 'WTXT'
typedef struct PACKED _WTXT_Header {
	uint32_t magic;			// [0x000] 'WTXT'
	uint32_t msg_tbl_name_offset;	// [0x004] Offset of message table name.
	uint32_t msg_tbl_count;		// [0x008] Number of strings in the message table.
} WTXT_Header;
```

The message table name offset points to the name of the message table,
encoded as Shift-JIS (code page 932).

Following the WTXT header is an array of message table pointers. This
table has the following general format for each entry:

```c
typedef struct PACKED _WTXT_MsgPointer {
	uint32_t msg_id_name_offset;	// [0x000] Offset of message name.
	uint32_t msg_offset;		// [0x004] Offset of message.
	uint32_t zero;			// [0x008] Zero. (NOTE: May not be present!)
} WTXT_MsgPointer;
```

* `msg_id_name_offset`: Offset of the message name. Encoded as Shift-JIS.
* `msg_offset`: Offset of the message text. Encoded as UTF-16.
  Endianness depends on file endianness.
* `zero`: Unused. **HOWEVER**, this may not be present in the offset table.

`msg_tbl_count` is equal to the total number of strings in the offset table,
assuming each string entry is exactly 12 bytes. For an unknown reason, many
strings are missing the `zero` field, so this isn't entirely accurate. Also,
some strings might only have a name and no text. The easiest way to determine
this is by checking if `msg_offset >= msg_tbl_name_offset`. This works because
the message text is all stored in one block, and the message names are stored
in one block *after* the message text.

Due to the occasionally missing `msg_offset` and `zero` fields, it is
impossible to accurately determine which string is which by simply reading the
WTXT offsets. The differential offset table must be parsed first.

## Differential Offset Table

In Sonic '06 files, this usually looks like "ABABABABABABAB", though sometimes
there's strings of "AAAAAAAA". Here's how to decode it.

Initial file position: 0x20 (32; size of header)
For each non-zero byte in the offset table:
* Convert the byte to binary: `'A' -> 0x41 -> 0100 0001`
* High two bits indicates data length:
  * `00`: End of offset table.
  * `01`: 6 bits.
  * `10`: 14 bits.
  * `11`: 30 bits.
* For `10` and `11`, concatenate the data from the next two or three bytes
  in the offset table. Note that these values might not be found in Sonic '06.
  (They're listed in the Sonic Forces BINA reference.)
* For the example 'A', we take the low 6 bits: `00 0001`
* Left-shift the data by 2: `0000 0100 == 0x04`
* Add the value to the current file position: `0x04 + 0x20 == 0x24`
* The first real offset is the 32-bit value located at 0x24 in the file.
  * Endianness depends on file endianness.
* Repeat to determine all real offsets until the end of the offset table is reached.

When parsing the offsets, some notes to keep in mind:
* The first offset is always the string table name, encoded in Shift-JIS.
* After the first offset, strings are stored using pairs of offsets:
  * String name (encoded in Shift-JIS)
  * String text (encoded in UTF-16)
* If the string text's offset is >= the offset of the string table name,
  this string doesn't actually have text. The string text offset should
  be considered the string name offset of the *next* message.

The end of the string table is aligned to a DWORD boundary, so extra `00`
bytes may be present. These can be ignored. (If writing an MST file, the
`00` bytes must be included for alignment, if necessary.)

## References

* HedgeLib BINA parser: https://github.com/Radfordhound/HedgeLib/blob/master/HedgeLib/Headers/BINAv1Header.cs
* Sonic Retro BINA documentation: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA
