/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * mst06.cpp: Main program.                                                *
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

#include <stdlib.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <string>
#include <vector>
using std::string;
using std::vector;
using std::pair;

#include "mst_structs.h"
#include "byteswap.h"
#include "tcharx.h"

int _tmain(int argc, TCHAR *argv[])
{
	if (argc != 2) {
		_ftprintf(stderr, _T("Syntax: %s mst_file.mst\n"), argv[0]);
		return EXIT_FAILURE;
	}

	FILE *f_mst = _tfopen(argv[1], _T("rb"));
	if (!f_mst) {
		_ftprintf(stderr, _T("*** ERROR opening %s: %s\n"), argv[1], _tcserror(errno));
		return EXIT_FAILURE;
	}

	// Read the MST header.
	MST_Header mst;
	size_t size = fread(&mst, 1, sizeof(mst), f_mst);
	if (size != sizeof(mst)) {
		// Read error.
		fprintf(stderr, "READ ERROR 1\n");
		fclose(f_mst);
		return EXIT_FAILURE;
	}

	// Check the BINA magic number.
	if (mst.bina_magic != cpu_to_be32(BINA_MAGIC)) {
		fprintf(stderr, "INVALID BINA MAGIC\n");
		fclose(f_mst);
		return EXIT_FAILURE;
	}

	// NOTE: Assuming big-endian for now.
	mst.file_size		= be32_to_cpu(mst.file_size);
	mst.offset_tbl_offset	= be32_to_cpu(mst.offset_tbl_offset);
	mst.offset_tbl_length	= be32_to_cpu(mst.offset_tbl_length);

	// Verify file size.
	if (mst.file_size < sizeof(MST_Header) + sizeof(WTXT_Header) + sizeof(WTXT_MsgPointer)) {
		// Sanity check: File is too small.
		fprintf(stderr, "FILE TOO SMALL\n");
		fclose(f_mst);
		return EXIT_FAILURE;
	} else if (mst.file_size > 16U*1024*1024) {
		// Sanity check: Must be 16 MB or less.
		fprintf(stderr, "FILE TOO BIG (>16MB)\n");
		fclose(f_mst);
		return EXIT_FAILURE;
	}

	// Verify offset table length and size.
	if ((uint64_t)sizeof(MST_Header) + (uint64_t)mst.offset_tbl_offset + (uint64_t)mst.offset_tbl_length > mst.file_size) {
		// Offset table error.
		fprintf(stderr, "OFFSET TABLE SIZE ERROR\n");
		fclose(f_mst);
		return EXIT_FAILURE;
	}

	// Read the entire file.
	uint8_t *mst_data = new uint8_t[mst.file_size];
	rewind(f_mst);
	size = fread(mst_data, 1, mst.file_size, f_mst);
	fclose(f_mst);
	if (size != mst.file_size) {
		fprintf(stderr, "READ ERROR 2\n");
		delete[] mst_data;
		return EXIT_FAILURE;
	}

	// Get pointers.
	const WTXT_Header *const pWtxt = reinterpret_cast<const WTXT_Header*>(&mst_data[sizeof(mst)]);
	const uint8_t *pOffsetTbl = &mst_data[sizeof(mst) + mst.offset_tbl_offset];
	const uint8_t *const pOffsetTblEnd = pOffsetTbl + mst.offset_tbl_length;

	// Calculate the offsets for each message.
	// Reference: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA

	// Message offset table.
	// The offset table has values that point into the message offset table.
	const uint8_t *pTbl = &mst_data[sizeof(mst)];
	const uint8_t *const pTblEnd = &mst_data[mst.file_size];

	vector<uint32_t> vMsgOffsets;
	bool doneOffsets = false;
	int i = 0;
	for (; pOffsetTbl < pOffsetTblEnd; pOffsetTbl++) {
		// High two bits of this byte indicate how long the offset is.
		if (i > 0) {
			printf("offset %d.%d: %c\n", (i-1)/2, (i-1)%2, (char)*pOffsetTbl);
		}
		i++;
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
					fprintf(stderr, "OFFSET TBL ERR on type 2\n");
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
					fprintf(stderr, "OFFSET TBL ERR on type 3\n");
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

		// Add the difference to pTbl.
		pTbl += offset_diff;
		if (pTbl + 3 >= pTblEnd) {
			// Out of bounds!
			fprintf(stderr, "pTbl OUT OF RANGE\n");
			delete[] mst_data;
			return EXIT_FAILURE;
		}

		// Read the 32-bit value at this offset.
		printf("- address: %08X\n", be32_to_cpu(*(uint32_t*)pTbl));
		vMsgOffsets.push_back(be32_to_cpu(*(uint32_t*)pTbl));
	}

	// We now have a vector of offset pairs:
	// - First offset: Message name.
	// - Second offset: Message text.

	// NOTE: First string is the string table name.
	// Get that one first.
	pTbl = &mst_data[sizeof(mst)];
	string tblName;
	do {
		const char *const pMsgName = reinterpret_cast<const char*>(&pTbl[vMsgOffsets[0]]);
		printf("offset: %04X\n", vMsgOffsets[0]);
		if (pMsgName >= reinterpret_cast<const char*>(pTblEnd)) {
			fprintf(stderr, "*** ERROR: MsgName for string table name is out of range.\n");
			break;
		}

		// TODO: Convert from cp1252 to UTF-8.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pTblEnd) - pMsgName);
		tblName = string(pMsgName, msgNameLen);
		printf("String table: %s\n", tblName.c_str());
	} while (0);

	// Temporary string for text.
	string msgText;
	msgText.reserve(1024);

	// Load the actual strings.
	// NOTE: Strings are NULL-terminated, so we have to determine the string length using strnlen().
	vector<pair<string, string> > vStrTbl;
	size_t msgNum = 0;
	for (size_t i = 1; i + 1 < vMsgOffsets.size(); i += 2, msgNum++) {
		const char *const pMsgName = reinterpret_cast<const char*>(&pTbl[vMsgOffsets[i]]);
		// TODO: Verify alignment.
		const char16_t *pMsgText = reinterpret_cast<const char16_t*>(&pTbl[vMsgOffsets[i+1]]);
		if (pMsgName >= reinterpret_cast<const char*>(pTblEnd)) {
			fprintf(stderr, "*** ERROR: MsgName for string %zu is out of range.\n", msgNum);
			break;
		} else if (pMsgText >= reinterpret_cast<const char16_t*>(pTblEnd)) {
			fprintf(stderr, "*** ERROR: MsgText for string %zu is out of range.\n", msgNum);
			break;
		}

		// Get the message name.
		// TODO: Convert from cp1252 to UTF-8.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pTblEnd) - pMsgName);
		string msgName(pMsgName, msgNameLen);

		msgText.clear();
		if (vMsgOffsets[i+1] >= vMsgOffsets[0]) {
			// WARNING: The message text is within the message name table.
			// This string doesn't have message text.
			// Seen in msg_town_mission_shadow.j.mst, message 126:
			// "rgba(255,153,0),color,rgba(255,153,0),color,rgba(255,153,0),color,rgba(255,153,0),color"
			// TODO: Does this need to be stored like this in order to function properly?
			printf("*** WARNING: MsgName with no MsgText...\n");
			i--;
		} else {
			// Get the message text.
			// TODO: Convert from UTF-16BE to UTF-8.
			// For now, simply discarding the high byte.
			for (; pMsgText < reinterpret_cast<const char16_t*>(pTblEnd); pMsgText++) {
				if (*pMsgText == 0) {
					// Found the NULL terminator.
					break;
				}
				msgText += (char)(be16_to_cpu(*pMsgText));
			}
		}

		printf("* Message %zu: %s -> %s\n", msgNum, msgName.c_str(), msgText.c_str());
		vStrTbl.push_back(std::make_pair(msgName, msgText));
	}

	return 0;
}
