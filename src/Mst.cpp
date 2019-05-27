/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * Mst.hpp: MST container class.                                           *
 *                                                                         *
 * Copyright (c) 2019 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-3.0-or-later                               *
 ***************************************************************************/

#include "Mst.hpp"

// C includes.
#include <stdint.h>

// C includes. (C++ namespace)
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>

#ifdef _MSC_VER
# define fseeko(stream, offset, whence) _fseeki64((stream), (offset), (whence))
# define ftello(stream) _ftelli64(stream)
#endif

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

// TODO: Check ENABLE_XML?
#include <tinyxml2.h>
using namespace tinyxml2;

Mst::Mst()
	: m_version('1')
	, m_isBigEndian(true)
{ }

/**
 * Load an MST string table.
 * @param filename MST string table filename.
 * @return 0 on success; negative POSIX error code on error.
 */
int Mst::loadMST(const TCHAR *filename)
{
	if (!filename || !filename[0]) {
		return -EINVAL;
	}

	FILE *f_mst = _tfopen(filename, _T("rb"));
	if (!f_mst) {
		// Error opening the file.
		return -errno;
	}
	int ret = loadMST(f_mst);
	fclose(f_mst);
	return ret;
}

/**
 * Load an MST string table.
 * @param fp MST string table file.
 * @return 0 on success; negative POSIX error code on error.
 */
int Mst::loadMST(FILE *fp)
{
	if (!fp) {
		return -EINVAL;
	}

	int err;

	// Clear the current string tables.
	m_name.clear();
	m_vStrTbl.clear();
	m_vStrLkup.clear();
	m_vDiffOffTbl.clear();
	m_version = '1';
	m_isBigEndian = true;

	// Read the MST header.
	MST_Header mst_header;
	size_t size = fread(&mst_header, 1, sizeof(mst_header), fp);
	if (size != sizeof(mst_header)) {
		// Read error.
		// TODO: Store more comprehensive error information.
		return -EIO;
	}

	// Check the BINA magic number.
	if (mst_header.bina_magic != cpu_to_be32(BINA_MAGIC)) {
		// TODO: Store more comprehensive error information.
		return -EIO;
	}

	// Check version number and endianness.
	if (mst_header.version != '1' || (mst_header.endianness != 'B' && mst_header.endianness != 'L')) {
		// Unsupported version and/or invalid endianness.
		// TODO: Store more comprehensive error information.
		return -EIO;
	}
	m_version = mst_header.version;
	m_isBigEndian = (mst_header.endianness == 'B');

	if (m_isBigEndian) {
		mst_header.file_size		= be32_to_cpu(mst_header.file_size);
		mst_header.doff_tbl_offset	= be32_to_cpu(mst_header.doff_tbl_offset);
		mst_header.doff_tbl_length	= be32_to_cpu(mst_header.doff_tbl_length);
	} else {
		mst_header.file_size		= le32_to_cpu(mst_header.file_size);
		mst_header.doff_tbl_offset	= le32_to_cpu(mst_header.doff_tbl_offset);
		mst_header.doff_tbl_length	= le32_to_cpu(mst_header.doff_tbl_length);
	}

	// Verify file size.
	if (mst_header.file_size < sizeof(MST_Header) + sizeof(WTXT_Header) + sizeof(WTXT_MsgPointer)) {
		// Sanity check: File is too small.
		// TODO: Store more comprehensive error information.
		return -EIO;
	} else if (mst_header.file_size > 16U*1024*1024) {
		// Sanity check: Must be 16 MB or less.
		// TODO: Store more comprehensive error information.
		return -EIO;
	}

	// Verify offset table length and size.
	if ((uint64_t)sizeof(MST_Header) + (uint64_t)mst_header.doff_tbl_offset + (uint64_t)mst_header.doff_tbl_length > mst_header.file_size) {
		// Offset table error.
		// TODO: Store more comprehensive error information.
		return -EIO;
	}

	// Read the entire file.
	// NOTE: Using a relative seek in case the file pointer was set by
	// the caller to not be at the beginning of the file.
	unique_ptr<uint8_t[]> mst_data(new uint8_t[mst_header.file_size]);
	fseeko(fp, -(off_t)(sizeof(mst_header)), SEEK_CUR);
	errno = 0;
	size = fread(mst_data.get(), 1, mst_header.file_size, fp);
	err = errno;
	if (size != mst_header.file_size) {
		// Short read.
		// TODO: Store more comprehensive error information.
		if (err != 0) {
			return -err;
		}
		return -EIO;
	}

	// Get pointers.
	const WTXT_Header *const pWtxt = reinterpret_cast<const WTXT_Header*>(&mst_data[sizeof(mst_header)]);
	const uint8_t *pDiffOffTbl = &mst_data[sizeof(mst_header) + mst_header.doff_tbl_offset];
	const uint8_t *const pDiffOffTblEnd = pDiffOffTbl + mst_header.doff_tbl_length;
	// Keep a copy of the differential offset table for later.
	m_vDiffOffTbl.resize(pDiffOffTblEnd - pDiffOffTbl);
	memcpy(m_vDiffOffTbl.data(), pDiffOffTbl, pDiffOffTblEnd - pDiffOffTbl);

	// Calculate the offsets for each message.
	// Reference: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA

	// Offset table.
	// The offset table has values that point into the message offset table.
	const uint8_t *pOffTbl = &mst_data[sizeof(mst_header)];
	const uint8_t *const pOffTblEnd = &mst_data[mst_header.file_size];

	// Use the differential offset table to get the
	// actual message offsets.
	vector<uint32_t> vMsgOffsets;
	bool doneOffsets = false;
	for (; pDiffOffTbl < pDiffOffTblEnd; pDiffOffTbl++) {
		// High two bits of this byte indicate how long the offset is.
		uint32_t offset_diff = 0;
		switch (*pDiffOffTbl >> 6) {
			case 0:
				// 0 bits long. End of offset table.
				doneOffsets = true;
				break;
			case 1:
				// 6 bits long.
				// Take low 6 bits of this byte and left-shift by 2.
				offset_diff = (pDiffOffTbl[0] & 0x3F) << 2;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 2:
				// 14 bits long.
				// Offset difference is stored in 2 bytes.
				if (pDiffOffTbl + 2 >= pDiffOffTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pDiffOffTbl[0] & 0x3F) << 10) |
				               (pDiffOffTbl[1] << 2);
				pDiffOffTbl++;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 3:
				// 30 bits long.
				// Offset difference is stored in 4 bytes.
				if (pDiffOffTbl + 4 >= pDiffOffTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pDiffOffTbl[0] & 0x3F) << 26) |
				               (pDiffOffTbl[1] << 18) |
				               (pDiffOffTbl[2] << 10) |
				               (pDiffOffTbl[3] << 2);
				pDiffOffTbl += 3;
				break;
		}

		if (doneOffsets)
			break;

		// Add the difference to pOffTbl.
		pOffTbl += offset_diff;
		if (pOffTbl + 3 >= pOffTblEnd) {
			// Out of bounds!
			// TODO: Store more comprehensive error information.
			return -EIO;
		}

		// Read the 32-bit value at this offset.
		uint32_t real_offset;
		if (m_isBigEndian) {
			real_offset = be32_to_cpu(*(uint32_t*)pOffTbl);
		} else {
			real_offset = le32_to_cpu(*(uint32_t*)pOffTbl);
		}
		vMsgOffsets.push_back(real_offset);
	}

	// We now have a vector of offset pairs:
	// - First offset: Message name.
	// - Second offset: Message text.

	// NOTE: First string is the string table name.
	// Get that one first.
	pOffTbl = &mst_data[sizeof(mst_header)];
	do {
		const char *const pMsgName = reinterpret_cast<const char*>(&pOffTbl[vMsgOffsets[0]]);
		if (pMsgName >= reinterpret_cast<const char*>(pOffTblEnd)) {
			// MsgName for the string table name is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pOffTblEnd) - pMsgName);
		m_name = cpN_to_utf8(932, pMsgName, static_cast<int>(msgNameLen));
	} while (0);

	// Temporary string for text.
	u16string msgText;

	// Load the actual strings.
	// NOTE: Strings are NULL-terminated, so we have to determine the string length using strnlen().
	size_t msgNum = 0;
	size_t idx = 0;	// String index.
	for (size_t i = 1; i + 1 < vMsgOffsets.size(); i += 2, msgNum++, idx++) {
		const char *const pMsgName = reinterpret_cast<const char*>(&pOffTbl[vMsgOffsets[i]]);
		// TODO: Verify alignment.
		const char16_t *pMsgText = reinterpret_cast<const char16_t*>(&pOffTbl[vMsgOffsets[i+1]]);

		if (pMsgName >= reinterpret_cast<const char*>(pOffTblEnd)) {
			// MsgName is out of range.
			// TODO: Store more comprehensive error information.
			break;
		} else if (pMsgText >= reinterpret_cast<const char16_t*>(pOffTblEnd)) {
			// MsgText is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		// Get the message name.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pOffTblEnd) - pMsgName);
		string msgName = cpN_to_utf8(932, pMsgName, static_cast<int>(msgNameLen));

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
				for (; pMsgText < reinterpret_cast<const char16_t*>(pOffTblEnd); pMsgText++) {
					if (*pMsgText == cpu_to_be16(0)) {
						// Found the NULL terminator.
						break;
					}
					msgText += static_cast<char16_t>(be16_to_cpu(*pMsgText));
				}
			} else {
				for (; pMsgText < reinterpret_cast<const char16_t*>(pOffTblEnd); pMsgText++) {
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
 * Custom XMLPrinter that uses tabs instead of spaces.
 */
class MstXMLPrinter : public XMLPrinter
{
	public:
		MstXMLPrinter(FILE *file = nullptr, bool compact = false, int depth = 0)
			: XMLPrinter(file, compact, depth) { }

	public:
		void PrintSpace(int depth) final
		{
			for (; depth > 0; depth--) {
				Putc('\t');
			}
		}
};

/**
 * Load an XML string table.
 * @param filename	[in] XML filename.
 * @param pVecErrs	[out,opt] Vector of user-readable error messages.
 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
 */
int Mst::loadXML(const TCHAR *filename, std::vector<std::string> *pVecErrs)
{
	if (!filename || !filename[0]) {
		return -EINVAL;
	}

	// Open the XML document.
	FILE *f_xml = _tfopen(filename, _T("r"));
	if (!f_xml) {
		// Error opening the XML document.
		return -errno;
	}
	int ret = loadXML(f_xml, pVecErrs);
	fclose(f_xml);
	return ret;
}

/**
 * Load an XML string table.
 * @param fp		[in] XML file.
 * @param pVecErrs	[out,opt] Vector of user-readable error messages.
 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
 */
int Mst::loadXML(FILE *fp, std::vector<std::string> *pVecErrs)
{
	if (!fp) {
		return -EINVAL;
	}

	// Clear the current string tables.
	m_name.clear();
	m_vStrTbl.clear();
	m_vStrLkup.clear();
	m_vDiffOffTbl.clear();
	m_version = '1';
	m_isBigEndian = true;

	// Parse the XML document.
	XMLDocument xml;
	int ret = xml.LoadFile(fp);
	if (ret != 0) {
		// Error parsing the XML document.
		if (pVecErrs) {
			const char *const errstr = xml.ErrorStr();
			if (errstr) {
				pVecErrs->push_back(errstr);
			}
		}
		return xml.ErrorID();
	}

	// TODO: Load m_vDiffOffTbl.

	// Get the root element: "mst06"
	XMLElement *const xml_mst06 = xml.FirstChildElement("mst06");
	if (!xml_mst06) {
		// No "mst06" element.
		if (pVecErrs) {
			pVecErrs->push_back("\"mst06\" element not found.");
		}
		return -EIO;
	}

	// Check if mst_version and endianness are set.
	// If they are, use them. If not, default to "1B".
	const char *const mst_version = xml_mst06->Attribute("mst_version");
	if (mst_version) {
		// If not "1", show a warning.
		if (strcmp(mst_version, "1") != 0) {
			if (pVecErrs) {
				pVecErrs->push_back("\"mst06\" mst_version is not \"1\". Continuing anyway.");
			}
		}
		m_version = mst_version[0];
	}
	const char *const mst_endianness = xml_mst06->Attribute("endianness");
	if (mst_endianness) {
		// If not "B" or "L", show a warning and assume "B".
		if ((mst_endianness[0] == 'B' || mst_endianness[0] == 'L') && mst_endianness[1] == '\0') {
			// Valid endianness.
			m_isBigEndian = (mst_endianness[0] == 'B');
		} else {
			// Endianness is not valid.
			// Default to big endian.
			if (pVecErrs) {
				char buf[256];
				snprintf(buf, sizeof(buf), "\"mst06\" endianness \"%s\" not recognized. Assuming big-endian.", mst_endianness);
				pVecErrs->push_back(buf);
			}
		}
	}

	// Get the string table name.
	const char *const strTblName = xml_mst06->Attribute("name");
	if (!strTblName) {
		// No "name" attribute.
		if (pVecErrs) {
			pVecErrs->push_back("\"mst06\" element has no \"name\" attribute.");
		}
		return -EIO;
	} else if (!strTblName[0]) {
		// "name" attribute is empty.
		if (pVecErrs) {
			pVecErrs->push_back("\"mst06\" element's \"name\" attribute is empty.");
		}
		return -EIO;
	}
	m_name = strTblName;

	// Read messages.
	// NOTE: Need to check for missing message indexes after.
	XMLElement *xml_msg = xml_mst06->FirstChildElement("message");
	if (!xml_msg) {
		// No messages.
		m_name.clear();
		if (pVecErrs) {
			pVecErrs->push_back("\"mst06\" element has no \"message\" elements.");
		}
		return -EIO;
	}

	for (; xml_msg != nullptr; xml_msg = xml_msg->NextSiblingElement("message")) {
		// Get the attributes.
		// TODO: Should errors here cause parsing to fail?
		char buf[256];

		// Index.
		unsigned int index = 0;
		XMLError err = xml_msg->QueryUnsignedAttribute("index", &index);
		switch (err) {
			case XML_SUCCESS:
				break;
			case XML_NO_ATTRIBUTE:
				if (pVecErrs) {
					snprintf(buf, sizeof(buf), "Line %d: \"message\" element has no \"index\" attribute.", xml_msg->GetLineNum());
					pVecErrs->push_back(buf);
				}
				break;
			case XML_WRONG_ATTRIBUTE_TYPE:
				if (pVecErrs) {
					snprintf(buf, sizeof(buf), "Line %d: \"message\" element's \"index\" attribute is not an unsigned integer.", xml_msg->GetLineNum());
					pVecErrs->push_back(buf);
				}
				break;
			default:
				if (pVecErrs) {
					snprintf(buf, sizeof(buf), "Line %d: Unknown error.", xml_msg->GetLineNum());
					pVecErrs->push_back(buf);
				}
				break;
		}
		if (err != XML_SUCCESS)
			continue;

		// Message name.
		const char *const msg_name = xml_msg->Attribute("name");
		if (!msg_name) {
			if (pVecErrs) {
				snprintf(buf, sizeof(buf), "Line %d: \"message\" element has no \"name\" attribute.", xml_msg->GetLineNum());
				pVecErrs->push_back(buf);
			}
			continue;
		} else if (!msg_name[0]) {
			if (pVecErrs) {
				snprintf(buf, sizeof(buf), "Line %d: \"message\" element has an empty \"name\" attribute.", xml_msg->GetLineNum());
				pVecErrs->push_back(buf);
			}
			continue;
		}

		// Message text.
		const char *msg_text = xml_msg->GetText();
		if (!msg_text) {
			msg_text = "";
		}

		// Check for a duplicated message.
		// If found, the original message will be replaced.
		if (index < m_vStrTbl.size()) {
			if (!m_vStrTbl[index].first.empty()) {
				// Found a duplicated message index.
				if (pVecErrs) {
					snprintf(buf, sizeof(buf), "Line %d: Duplicate message index %u. This message will supercede the previous message.", xml_msg->GetLineNum(), index);
					pVecErrs->push_back(buf);
				}

				// Remove the string from m_vStrLkup.
				m_vStrLkup.erase(m_vStrTbl[index].first);
			}
		}

		// Add the message to the main table.
		if (index >= m_vStrTbl.size()) {
			// Need to resize the main table.
			m_vStrTbl.resize(index+1);
		}
		m_vStrTbl[index].first = msg_name;
		// TODO: utf8_to_utf16() overload that takes `const char*`?
		m_vStrTbl[index].second = unescape(utf8_to_utf16(msg_text, strlen(msg_text)));

		// Add the message to the lookup table.
		m_vStrLkup.insert(std::make_pair(msg_name, index));
	}

	// TODO: Check for missing message indexes.

	// Document processed.
	return 0;
}

/**
 * Save the string table as MST.
 * @param filename MST filename.
 * @return 0 on success; negative POSIX error code on error.
 */
int Mst::saveMST(const TCHAR *filename) const
{
	if (!filename || !filename[0]) {
		return -EINVAL;
	} else if (m_vStrTbl.empty()) {
		return -ENODATA;	// TODO: Better error code?
	}

	FILE *f_mst = _tfopen(filename, _T("wb"));
	if (!f_mst) {
		// Error opening the MST file.
		return -errno;
	}
	int ret = saveMST(f_mst);
	fclose(f_mst);
	return ret;
}

/**
 * Save the string table as XML.
 * @param fp XML file.
 * @return 0 on success; negative POSIX error code on error.
 */
int Mst::saveMST(FILE *fp) const
{
	// BEFORE MST COMMIT: Check here!
	if (m_vStrTbl.empty()) {
		return -ENODATA;	// TODO: Better error code?
	}

	// MST header.
	// NOTE: Parts of the header can't be filled in until
	// the rest of the string table is handled.
	MST_Header mst_header;
	memset(&mst_header, 0, sizeof(mst_header));
	mst_header.version = m_version;
	mst_header.endianness = (m_isBigEndian ? 'B' : 'L');
	mst_header.bina_magic = cpu_to_be32(BINA_MAGIC);

	// WTXT header.
	// NOTE: Most of this header is filled out later.
	WTXT_Header wtxt_header;
	wtxt_header.magic = cpu_to_be32(WTXT_MAGIC);
	wtxt_header.msg_tbl_name_offset = 0;
	wtxt_header.msg_tbl_count = 0;

	// Data vectors.
	// NOTE: vOffsetTbl will have offsets relative to the
	// beginning of each vMsgNames and vMsgText in the first run.
	// The base addresses will be added in the second run.
	// vOffsetTblType will be used to determine the base address.
	vector<uint32_t> vOffsetTbl;	// Primary offset table.
	vector<uint8_t> vOffsetTblType;	// Type of entry for each offset: 0=zero, 1=text, 2=name
	vector<char16_t> vMsgText;	// Message text.
	vector<char> vMsgNames;		// Message names.
	// TODO: Use m_vDiffOffTbl.
	vector<char> vDiffOffTbl;	// Differential offset table.

	// TODO: Better size reservations.
	vOffsetTbl.reserve(m_vStrTbl.size() * 3);
	vOffsetTblType.reserve(m_vStrTbl.size() * 3);
	vMsgText.reserve(m_vStrTbl.size() * 32);
	vMsgNames.reserve(m_vStrTbl.size() * 32);
	vDiffOffTbl.reserve((m_vStrTbl.size() + 3) & ~(size_t)(3U));

	// For strings with both name and text, three offsets will be stored:
	// - Name offset
	// - Text offset
	// - Zero

	// For strings with only name, only the name offset will be stored.
	// TODO: Compare to MST files and see if maybe a zero is stored too?

	bool hasAZero = false;	// If true, skip 8 bytes for the next offset.

	// String table name.
	// NOTE: While this is part of the names table, the offset is stored
	// in the WTXT header, *not* the offset table, and it's not included
	// in the differential offset table.
	{
		if (!m_name.empty()) {
			const size_t name_size = m_name.size();
			// +1 for NULL terminator.
			vMsgNames.resize(name_size + 1);
			memcpy(vMsgNames.data(), m_name.c_str(), name_size+1);
		} else {
			// Empty string table name...
			// TODO: Report a warning.
			const char empty_name[] = "mst06_generic_name";
			vMsgNames.resize(sizeof(empty_name));
			memcpy(vMsgNames.data(), empty_name, sizeof(empty_name));
		}

		// Skip 4 bytes from the beginning of the WTXT header.
		vDiffOffTbl.push_back('A');

		// Next offset will skip 8 bytes to skip over the string count.
		hasAZero = true;
	}

	// Host endianness.
	static const bool hostIsBigEndian = (SYS_BYTEORDER == SYS_BIG_ENDIAN);
	const bool hostMatchesFileEndianness = (hostIsBigEndian == m_isBigEndian);

	size_t i = 0;
	for (auto iter = m_vStrTbl.cbegin(); iter != m_vStrTbl.cend(); ++iter, ++i) {
		const size_t name_pos = vMsgNames.size();
		const size_t text_pos = vMsgText.size();

		// Copy the message name.
		if (!iter->first.empty()) {
			// Convert to Shift-JIS first.
			// TODO: Show warnings for strings with characters that
			// can't be converted to Shift-JIS?
			const string sjis_str = utf8_to_cpN(932, iter->first.data(), (int)iter->first.size());
			// Copy the message name into the vector.
			const size_t name_size = sjis_str.size();
			// +1 for NULL terminator.
			vMsgNames.resize(name_pos + name_size + 1);
			memcpy(&vMsgNames[name_pos], sjis_str.c_str(), name_size+1);
		} else {
			// Empty message name...
			// TODO: Report a warning.
			char buf[64];
			int len = snprintf(buf, sizeof(buf), "XXX_MSG_%zu", i);
			// +1 for NULL terminator.
			vMsgNames.resize(name_pos + len + 1);
			memcpy(&vMsgNames[name_pos], buf, len+1);
		}
		assert(name_pos <= 16U*1024*1024);
		vOffsetTbl.push_back(static_cast<uint32_t>(name_pos));
		vOffsetTblType.push_back(2);	// Name
		// Difference of 4 or 8 bytes, depending on whether or not
		// a zero offset was added previously.
		vDiffOffTbl.push_back(hasAZero ? 'B' : 'A');
		hasAZero = false;

		// Copy the message text.
		// TODO: Add support for writing little-endian files?
		if (!iter->second.empty()) {
			// Copy the message text into the vector.
			u16string msg_text;
			if (hostMatchesFileEndianness) {
				// Host endianness matches file endianness.
				// No conversion is necessary.
				// TODO: Can we eliminate this copy?
				msg_text = iter->second;
			} else {
				// Host byteorder does not match file endianness.
				// Swap it.
				msg_text = utf16_bswap(iter->second.data(), iter->second.size());
			}

			const size_t msg_size = msg_text.size();
			vMsgText.resize(text_pos + msg_size + 1);
			// +1 for NULL terminator.
			memcpy(&vMsgText[text_pos], msg_text.c_str(), (msg_size+1) * sizeof(char16_t));

			// NOTE: Text offset must be mutliplied by sizeof(char16_t),
			// since vMsgText is char16_t, but vOffsetTbl uses bytes.
			assert(text_pos <= 16U*1024*1024);
			vOffsetTbl.push_back(static_cast<uint32_t>(text_pos * sizeof(char16_t)));
			vOffsetTbl.push_back(0);	// Zero entry.
			vOffsetTblType.push_back(1);	// Text
			vOffsetTblType.push_back(0);	// Zero

			// Difference of 4 bytes from the previous offset,
			// and we now have a zero offset.
			vDiffOffTbl.push_back('A');
			hasAZero = true;
		}
	}

	// Determine the message table base addresses.
	const uint32_t text_tbl_base = static_cast<uint32_t>(sizeof(wtxt_header) + (vOffsetTbl.size() * sizeof(uint32_t)));
	const uint32_t name_tbl_base = static_cast<uint32_t>(text_tbl_base + (vMsgText.size() * sizeof(char16_t)));

	// Differential offset table must be DWORD-aligned for both
	// starting offset and length.
	uint32_t doff_tbl_offset = static_cast<uint32_t>(name_tbl_base + vMsgNames.size());
	if (doff_tbl_offset & 3) {
		// Need to align the starting offset to a multiple of 4.
		vMsgNames.resize(vMsgNames.size() + (4 - (doff_tbl_offset & 3)));
		doff_tbl_offset = static_cast<uint32_t>(name_tbl_base + vMsgNames.size());
	}
	uint32_t doff_tbl_length = static_cast<uint32_t>(vDiffOffTbl.size());
	if (doff_tbl_length & 3) {
		// Need to align the size to a multiple of 4.
		vDiffOffTbl.resize((vDiffOffTbl.size() + 3) & ~(size_t)(3U));
		doff_tbl_length = static_cast<uint32_t>(vDiffOffTbl.size());
	}

	// Update WTXT_Header.
	wtxt_header.msg_tbl_name_offset = cpu_to_be32(name_tbl_base);
	// TODO: Make sure the offset table's size is a multiple of 3 uint32_t's. (12 bytes)
	const uint32_t msg_tbl_count = static_cast<uint32_t>(vOffsetTbl.size() / 3);
	wtxt_header.msg_tbl_count = cpu_to_be32(msg_tbl_count);

	// Update the offset table base addresses.
	i = 0;
	for (auto iter = vOffsetTbl.begin(); iter != vOffsetTbl.end(); ++iter, ++i) {
		switch (vOffsetTblType[i]) {
			case 0:
				// Zero entry.
				break;
			case 1:
				// Text entry.
				*iter += text_tbl_base;
				break;
			case 2:
				// Name entry.
				*iter += name_tbl_base;
				break;
			default:
				// Invalid entry.
				assert(!"Unexpected offset table type.");
				break;
		}

		if (!hostMatchesFileEndianness) {
			// Byteswap the offset.
			*iter = __swab32(*iter);
		}
	}

	// Update the MST header.
	if (hostMatchesFileEndianness) {
		// Endianness matches. No conversion is necessary.
		mst_header.file_size = sizeof(mst_header) + doff_tbl_offset + doff_tbl_length;
		mst_header.doff_tbl_offset = doff_tbl_offset;
		mst_header.doff_tbl_length = doff_tbl_length;
	} else {
		// Endianness does not match. Byteswap!
		mst_header.file_size = __swab32(sizeof(mst_header) + doff_tbl_offset + doff_tbl_length);
		mst_header.doff_tbl_offset = __swab32(doff_tbl_offset);
		mst_header.doff_tbl_length = __swab32(doff_tbl_length);
	}

	// Write everything to the file.
	errno = 0;
	size_t size = fwrite(&mst_header, 1, sizeof(mst_header), fp);
	if (size != sizeof(mst_header)) {
		return (errno ? -errno : -EIO);
	}
	errno = 0;
	size = fwrite(&wtxt_header, 1, sizeof(wtxt_header), fp);
	if (size != sizeof(wtxt_header)) {
		return (errno ? -errno : -EIO);
	}
	errno = 0;
	size = fwrite(vOffsetTbl.data(), sizeof(uint32_t), vOffsetTbl.size(), fp);
	if (size != vOffsetTbl.size()) {
		return (errno ? -errno : -EIO);
	}
	errno = 0;
	size = fwrite(vMsgText.data(), sizeof(char16_t), vMsgText.size(), fp);
	if (size != vMsgText.size()) {
		return (errno ? -errno : -EIO);
	}
	errno = 0;
	size = fwrite(vMsgNames.data(), 1, vMsgNames.size(), fp);
	if (size != vMsgNames.size()) {
		return (errno ? -errno : -EIO);
	}
	errno = 0;
	size = fwrite(vDiffOffTbl.data(), 1, vDiffOffTbl.size(), fp);
	if (size != vDiffOffTbl.size()) {
		return (errno ? -errno : -EIO);
	}

	// We're done here.
	return 0;
}

/**
 * Save the string table as XML.
 * @param filename XML filename.
 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
 */
int Mst::saveXML(const TCHAR *filename) const
{
	if (!filename || !filename[0]) {
		return -EINVAL;
	} else if (m_vStrTbl.empty()) {
		return -ENODATA;	// TODO: Better error code?
	}

	FILE *f_xml = _tfopen(filename, _T("w"));
	if (!f_xml) {
		// Error opening the XML file.
		return -errno;
	}
	int ret = saveXML(f_xml);
	fclose(f_xml);
	return ret;
}

/**
 * Save the string table as XML.
 * @param fp XML file.
 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
 */
int Mst::saveXML(FILE *fp) const
{
	// BEFORE MST COMMIT: Check here!
	if (m_vStrTbl.empty()) {
		return -ENODATA;	// TODO: Better error code?
	}

	// Create an XML document.
	XMLDocument xml;
	XMLDeclaration *const xml_decl = xml.NewDeclaration();
	xml.InsertFirstChild(xml_decl);

	XMLElement *const xml_mst06 = xml.NewElement("mst06");
	xml.InsertEndChild(xml_mst06);
	xml_mst06->SetAttribute("name", m_name.c_str());
	const char verstr[2] = {m_version, '\0'};
	xml_mst06->SetAttribute("mst_version", verstr);
	xml_mst06->SetAttribute("endianness", (m_isBigEndian ? "B" : "L"));

	size_t i = 0;
	for (auto iter = m_vStrTbl.cbegin(); iter != m_vStrTbl.cend(); ++iter, ++i) {
		XMLElement *const xml_msg = xml.NewElement("message");
		xml_mst06->InsertEndChild(xml_msg);
		xml_msg->SetAttribute("index", static_cast<unsigned int>(i));
		xml_msg->SetAttribute("name", iter->first.c_str());
		if (!iter->second.empty()) {
			xml_msg->SetText(escape(utf16_to_utf8(iter->second)).c_str());
		}
	}

	// Save the differential offset table.
	// FIXME: Show an error if the differential offset table is empty.
	const string diffOffTbl = escapeDiffOffTbl(m_vDiffOffTbl.data(), m_vDiffOffTbl.size());
	XMLElement *const xml_diffOffTbl = xml.NewElement("DiffOffTbl");
	xml_mst06->InsertEndChild(xml_diffOffTbl);
	xml_diffOffTbl->SetText(diffOffTbl.c_str());

	// Save the XML document.
	// NOTE: Using our custom XMLPrinter for tabs instead of spaces.
	MstXMLPrinter stream(fp, false);
	xml.Print(&stream);
	return xml.ErrorID();
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

/** Accessors **/

/**
 * Get a string's text. (UTF-8)
 * @param idx String index.
 * @return String text. (UTF-8)
 */
string Mst::strText_utf8(size_t index)
{
	if (index >= m_vStrTbl.size())
		return string();
	return utf16_to_utf8(m_vStrTbl[index].second);
}

/**
 * Get a string's text. (UTF-8)
 * @param idx String name. (UTF-8)
 * @return String text. (UTF-8)
 */
string Mst::strText_utf8(const string &name)
{
	auto iter = m_vStrLkup.find(name);
	if (iter == m_vStrLkup.end()) {
		// Not found.
		return string();
	}
	return strText_utf8(iter->second);
}

/**
 * Get a string's text. (UTF-16)
 * @param idx String index.
 * @return String text. (UTF-16)
 */
u16string Mst::strText_utf16(size_t index)
{
	if (index >= m_vStrTbl.size())
		return u16string();
	return m_vStrTbl[index].second;
}

/**
 * Get a string's text. (UTF-16)
 * @param idx String name. (UTF-8)
 * @return String text. (UTF-16)
 */
u16string Mst::strText_utf16(const string &name)
{
	auto iter = m_vStrLkup.find(name);
	if (iter == m_vStrLkup.end()) {
		// Not found.
		return u16string();
	}
	return strText_utf16(iter->second);
}

/** String escape functions **/

/**
 * Escape a UTF-8 string.
 * @param str Unescaped string.
 * @return Escaped string.
 */
string Mst::escape(const string &str)
{
	string ret;
	ret.reserve(str.size() + 8);
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		switch (*iter) {
			case '\\':
				ret += "\\\\";
				break;
			case '\n':
				ret += "\\n";
				break;
			case '\f':
				ret += "\\f";
				break;
			default:
				ret += *iter;
				break;
		}
	}
	return ret;
}

/**
 * Escape a UTF-16 string.
 * @param str Unescaped string.
 * @return Escaped string.
 */
u16string Mst::escape(const u16string &str)
{
	u16string ret;
	ret.reserve(str.size() + 8);
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		switch (*iter) {
			case '\\':
				ret += u"\\\\";
				break;
			case '\n':
				ret += u"\\\n";
				break;
			case '\f':
				ret += u"\\\f";
				break;
			default:
				ret += *iter;
				break;
		}
	}
	return ret;
}

/**
 * Unescape a UTF-8 string.
 * @param str Escaped string.
 * @return Unescaped string.
 */
string Mst::unescape(const string &str)
{
	string ret;
	ret.reserve(str.size());
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		if (*iter != '\\') {
			// Not an escape character.
			ret += *iter;
			continue;
		}

		// Escape character.
		++iter;
		if (iter == str.cend()) {
			// Backslash at the end of the string.
			ret += '\\';
			break;
		}
		switch (*iter) {
			case '\\':
				ret += '\\';
				break;
			case 'n':
				ret += '\n';
				break;
			case 'f':
				ret += '\f';
				break;
			default:
				// Invalid escape sequence.
				ret += '\\';
				ret += *iter;
				break;
		}
	}
	return ret;
}

/**
 * Unescape a UTF-16 string.
 * @param str Escaped string.
 * @return Unscaped string.
 */
u16string Mst::unescape(const u16string &str)
{
	u16string ret;
	ret.reserve(str.size());
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		if (*iter != u'\\') {
			// Not an escape character.
			ret += *iter;
			continue;
		}

		// Escape character.
		++iter;
		if (iter == str.cend()) {
			// Backslash at the end of the string.
			ret += u'\\';
			break;
		}
		switch (*iter) {
			case '\\':
				ret += u'\\';
				break;
			case 'n':
				ret += u'\n';
				break;
			case 'f':
				ret += u'\f';
				break;
			default:
				// Invalid escape sequence.
				ret += u'\\';
				ret += *iter;
				break;
		}
	}
	return ret;
}

/**
 * Format a differential offset table as an XML-compatible string.
 * @param diffTbl Differential offset table.
 * @param len Length.
 * @return XML-compatible string.
 */
string Mst::escapeDiffOffTbl(const uint8_t *diffTbl, size_t len)
{
	string ret;
	ret.reserve(len+16);
	for (; len > 0; diffTbl++, len--) {
		if (*diffTbl < 0x20 || *diffTbl >= 0x7F) {
			// Escape the character.
			char buf[8];
			snprintf(buf, sizeof(buf), "\\x%02X", *diffTbl);
			ret += buf;
		} else {
			// Use the character as-is.
			ret += *diffTbl;
		}
	}
	return ret;
}
