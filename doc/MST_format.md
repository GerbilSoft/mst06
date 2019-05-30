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
table has the following format for each entry:

```c
typedef struct PACKED _WTXT_MsgPointer {
	uint32_t name_offset;		// [0x000] Offset of message name. (Shift-JIS)
	uint32_t text_offset;		// [0x004] Offset of message text. (UTF-16)
	uint32_t placeholder_offset;	// [0x008] If non-zero, offset of placeholder icon name. (Shift-JIS)
} WTXT_MsgPointer;
```

* `name_offset`: Offset of the message name. Encoded as Shift-JIS.
* `text_offset`: Offset of the message text. Encoded as UTF-16.
  Endianness depends on file endianness.
* `placeholder_offset`: If non-zero, this indicates the offset of the
  placeholder offset. Usually this is a button icon name, though sometimes
  it can be an "rgb" string. Encoded as Shift-JIS.

`msg_tbl_count` is equal to the total number of strings in the offset table.

## Differential Offset Table

In Sonic '06 files, the differential offset table is present and allows for
parsing the main offset table while skipping zero offsets, e.g. for strings
that don't have placeholder names. This table usually looks like
"ABABABABABABAB", though in cases where a placeholder name is present, there
may be sections of "AAAAAAAA". Here's how to decode it.

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

The end of the string table is aligned to a DWORD boundary, so extra `00`
bytes may be present. These can be ignored. (If writing an MST file, the
`00` bytes must be included for alignment, if necessary.)

This table is basically redundant, since you can just read the main offset
table to get the correct information. Sonic '06 **requires** this table to
be correct, though; otherwise, it will crash. `mst06` treats this table as
write-only; that is, it's not used when parsing MST files, but it *is* written
when converting XML to MST.

## References

* HedgeLib BINA parser: https://github.com/Radfordhound/HedgeLib/blob/master/HedgeLib/Headers/BINAv1Header.cs
* Sonic Retro BINA documentation: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA
