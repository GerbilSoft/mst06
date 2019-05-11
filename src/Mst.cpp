/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * Mst.hpp: MST container class.                                           *
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

#include "Mst.hpp"

// C includes.
#include <stdint.h>

// C includes. (C++ namespace)
#include <cerrno>
#include <cstring>

// C++ includes.
#include <memory>
#include <string>
using std::u16string;
using std::unique_ptr;
using std::string;
using std::vector;

#include "mst_structs.h"
#include "byteswap.h"

// Text encoding functions.
#include "TextFuncs.hpp"

Mst::Mst()
	: m_version(1)
	, m_isBigEndian(true)
{ }

/**
 * Load an MST string table.
 * @param filename MST string table filename.
 * @return 0 on success; negative POSIX error code on error.
 */
int Mst::loadMST(const TCHAR *filename)
{
	int err;

	// Clear the current string tables.
	m_name.clear();
	m_vStrTbl.clear();
	m_vStrLkup.clear();

	FILE *f_mst = _tfopen(filename, _T("rb"));
	if (!f_mst) {
		// Error opening the file.
		return -errno;
	}

	// Read the MST header.
	MST_Header mst;
	size_t size = fread(&mst, 1, sizeof(mst), f_mst);
	if (size != sizeof(mst)) {
		// Read error.
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	}

	// Check the BINA magic number.
	if (mst.bina_magic != cpu_to_be32(BINA_MAGIC)) {
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	}

	// Check version number and endianness.
	if (mst.version != '1' || (mst.endianness != 'B' && mst.endianness != 'L')) {
		// Unsupported version and/or invalid endianness.
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	}
	m_version = 1;
	m_isBigEndian = (mst.endianness == 'B');

	if (m_isBigEndian) {
		mst.file_size		= be32_to_cpu(mst.file_size);
		mst.offset_tbl_offset	= be32_to_cpu(mst.offset_tbl_offset);
		mst.offset_tbl_length	= be32_to_cpu(mst.offset_tbl_length);
	} else {
		mst.file_size		= le32_to_cpu(mst.file_size);
		mst.offset_tbl_offset	= le32_to_cpu(mst.offset_tbl_offset);
		mst.offset_tbl_length	= le32_to_cpu(mst.offset_tbl_length);
	}

	// Verify file size.
	if (mst.file_size < sizeof(MST_Header) + sizeof(WTXT_Header) + sizeof(WTXT_MsgPointer)) {
		// Sanity check: File is too small.
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	} else if (mst.file_size > 16U*1024*1024) {
		// Sanity check: Must be 16 MB or less.
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	}

	// Verify offset table length and size.
	if ((uint64_t)sizeof(MST_Header) + (uint64_t)mst.offset_tbl_offset + (uint64_t)mst.offset_tbl_length > mst.file_size) {
		// Offset table error.
		// TODO: Store more comprehensive error information.
		fclose(f_mst);
		return -EIO;
	}

	// Read the entire file.
	unique_ptr<uint8_t[]> mst_data(new uint8_t[mst.file_size]);
	rewind(f_mst);
	errno = 0;
	size = fread(mst_data.get(), 1, mst.file_size, f_mst);
	err = errno;
	fclose(f_mst);
	if (size != mst.file_size) {
		// Short read.
		// TODO: Store more comprehensive error information.
		if (err != 0) {
			return -err;
		}
		return -EIO;
	}

	// Get pointers.
	const WTXT_Header *const pWtxt = reinterpret_cast<const WTXT_Header*>(&mst_data[sizeof(mst)]);
	const uint8_t *pOffsetTbl = &mst_data[sizeof(mst) + mst.offset_tbl_offset];
	const uint8_t *const pOffsetTblEnd = pOffsetTbl + mst.offset_tbl_length;

	// Calculate the offsets for each message.
	// Reference: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA

	// Differential offset table.
	// The offset table has values that point into the message offset table.
	const uint8_t *pDiffTbl = &mst_data[sizeof(mst)];
	const uint8_t *const pDiffTblEnd = &mst_data[mst.file_size];

	// Use the differential offset table to get the
	// actual message offsets.
	vector<uint32_t> vMsgOffsets;
	bool doneOffsets = false;
	for (; pOffsetTbl < pOffsetTblEnd; pOffsetTbl++) {
		// High two bits of this byte indicate how long the offset is.
		uint32_t offset_diff = 0;
		switch (*pOffsetTbl >> 6) {
			case 0:
				// 0 bits long. End of offset table.
				doneOffsets = true;
				break;
			case 1:
				// 6 bits long.
				// Take low 6 bits of this byte and left-shift by 2.
				offset_diff = (pOffsetTbl[0] & 0x3F) << 2;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 2:
				// 14 bits long.
				// Offset difference is stored in 2 bytes.
				if (pOffsetTbl + 2 >= pOffsetTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pOffsetTbl[0] & 0x3F) << 10) |
				               (pOffsetTbl[1] << 2);
				pOffsetTbl++;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 3:
				// 30 bits long.
				// Offset difference is stored in 4 bytes.
				if (pOffsetTbl + 4 >= pOffsetTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pOffsetTbl[0] & 0x3F) << 26) |
				               (pOffsetTbl[1] << 18) |
				               (pOffsetTbl[2] << 10) |
				               (pOffsetTbl[3] << 2);
				pOffsetTbl += 3;
				break;
		}

		if (doneOffsets)
			break;

		// Add the difference to pDiffTbl.
		pDiffTbl += offset_diff;
		if (pDiffTbl + 3 >= pDiffTblEnd) {
			// Out of bounds!
			// TODO: Store more comprehensive error information.
			return -EIO;
		}

		// Read the 32-bit value at this offset.
		uint32_t real_offset;
		if (m_isBigEndian) {
			real_offset = be32_to_cpu(*(uint32_t*)pDiffTbl);
		} else {
			real_offset = le32_to_cpu(*(uint32_t*)pDiffTbl);
		}
		vMsgOffsets.push_back(real_offset);
	}

	// We now have a vector of offset pairs:
	// - First offset: Message name.
	// - Second offset: Message text.

	// NOTE: First string is the string table name.
	// Get that one first.
	pDiffTbl = &mst_data[sizeof(mst)];
	do {
		const char *const pMsgName = reinterpret_cast<const char*>(&pDiffTbl[vMsgOffsets[0]]);
		if (pMsgName >= reinterpret_cast<const char*>(pDiffTblEnd)) {
			// MsgName for the string table name is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		// TODO: Convert from Shift-JIS to UTF-8.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pDiffTblEnd) - pMsgName);
		m_name = string(pMsgName, msgNameLen);
	} while (0);

	// Temporary string for text.
	u16string msgText;

	// Load the actual strings.
	// NOTE: Strings are NULL-terminated, so we have to determine the string length using strnlen().
	size_t msgNum = 0;
	size_t idx = 0;	// String index.
	for (size_t i = 1; i + 1 < vMsgOffsets.size(); i += 2, msgNum++, idx++) {
		const char *const pMsgName = reinterpret_cast<const char*>(&pDiffTbl[vMsgOffsets[i]]);
		// TODO: Verify alignment.
		const char16_t *pMsgText = reinterpret_cast<const char16_t*>(&pDiffTbl[vMsgOffsets[i+1]]);

		if (pMsgName >= reinterpret_cast<const char*>(pDiffTblEnd)) {
			// MsgName is out of range.
			// TODO: Store more comprehensive error information.
			break;
		} else if (pMsgText >= reinterpret_cast<const char16_t*>(pDiffTblEnd)) {
			// MsgText is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		// Get the message name.
		// TODO: Convert from Shift-JIS to UTF-8.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pDiffTblEnd) - pMsgName);
		string msgName(pMsgName, msgNameLen);

		msgText.clear();
		if (vMsgOffsets[i+1] >= vMsgOffsets[0]) {
			// WARNING: The message text is within the message name table.
			// This string doesn't have message text.
			// Seen in msg_town_mission_shadow.j.mst, message 126:
			// "rgba(255,153,0),color,rgba(255,153,0),color,rgba(255,153,0),color,rgba(255,153,0),color"
			// TODO: Does this need to be stored like this in order to function properly?
			i--;
		} else {
			// Find the end of the message text.
			size_t len = 0;
			if (m_isBigEndian) {
				for (; pMsgText < reinterpret_cast<const char16_t*>(pDiffTblEnd); pMsgText++) {
					if (*pMsgText == cpu_to_be16(0)) {
						// Found the NULL terminator.
						break;
					}
					msgText += static_cast<char16_t>(be16_to_cpu(*pMsgText));
				}
			} else {
				for (; pMsgText < reinterpret_cast<const char16_t*>(pDiffTblEnd); pMsgText++) {
					if (*pMsgText == cpu_to_le16(0)) {
						// Found the NULL terminator.
						break;
					}
					msgText += static_cast<char16_t>(le16_to_cpu(*pMsgText));
				}
			}
		}

		// Save the string table entry.
		// NOTE: Saving entries for empty strings, too.
		m_vStrTbl.emplace_back(std::make_pair(msgName, std::move(msgText)));
		m_vStrLkup.insert(std::make_pair(std::move(msgName), idx));
	}

	// We're done here.
	return 0;
}

/**
 * Dump the string table to stdout.
 */
void Mst::dump(void) const
{
	printf("String table: %s\n", m_name.c_str());
	size_t idx = 0;
	for (auto iter = m_vStrTbl.cbegin(); iter != m_vStrTbl.cend(); ++iter, ++idx) {
		printf("* Message %zu: %s -> ", idx, iter->first.c_str());

		// Convert the message text from UTF-16 to UTF-8.
		// TODO: Escape newlines and form feeds?
		printf("%s\n", utf16_to_utf8(iter->second).c_str());
	}
}
