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
#include <cctype>
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
#include <unordered_map>
using std::u16string;
using std::unique_ptr;
using std::unordered_map;
using std::string;
using std::vector;

#include "mst_structs.h"
#include "byteswap.h"

// Text encoding functions.
#include "TextFuncs.hpp"

// TODO: Check ENABLE_XML?
#include <tinyxml2.h>
using namespace tinyxml2;

// Invalid offset value.
#define INVALID_OFFSET ~0U

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
	m_mapPlaceholder.clear();
	m_vStrLkup.clear();
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
	// NOTE: The differential offset table is NOT used for loading,
	// since it's basically redundant information.
	const WTXT_Header *const pWtxt = reinterpret_cast<const WTXT_Header*>(&mst_data[sizeof(mst_header)]);

	// Calculate the offsets for each message.
	// Reference: https://info.sonicretro.org/SCHG:Sonic_Forces/Formats/BINA

	// Offset table.
	// The offset table has values that point into the message offset table.
	const uint8_t *const pOffTblU8 = &mst_data[sizeof(mst_header)];
	const uint8_t *const pOffTblEndU8 = &mst_data[mst_header.file_size];
	const WTXT_Header *const pWtxtHeader = reinterpret_cast<const WTXT_Header*>(pOffTblU8);
	const WTXT_MsgPointer *pOffTbl = reinterpret_cast<const WTXT_MsgPointer*>(pOffTblU8 + sizeof(WTXT_Header));
	const WTXT_MsgPointer *const pOffTblEnd = reinterpret_cast<const WTXT_MsgPointer*>(pOffTblEndU8);

	static const bool hostIsBigEndian = (SYS_BYTEORDER == SYS_BIG_ENDIAN);
	const bool hostMatchesFileEndianness = (hostIsBigEndian == m_isBigEndian);

	// NOTE: First string is the string table name.
	// Get that one first.
	do {
		uint32_t name_offset = pWtxtHeader->msg_tbl_name_offset;
		if (!hostMatchesFileEndianness) {
			name_offset = __swab32(name_offset);
		}

		const char *const pMsgName = reinterpret_cast<const char*>(&pOffTblU8[name_offset]);
		if (pMsgName >= reinterpret_cast<const char*>(pOffTblEndU8)) {
			// MsgName for the string table name is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pOffTblEndU8) - pMsgName);
		m_name = cpN_to_utf8(932, pMsgName, static_cast<int>(msgNameLen));
	} while (0);

	// Temporary string for text.
	u16string msgText;

	// Load the actual strings.
	// NOTE: Strings are NULL-terminated, so we have to determine the string length using strnlen().
	size_t idx = 0;	// String index.
	for (const WTXT_MsgPointer *p = pOffTbl; p < pOffTblEnd; p++, idx++) {
		WTXT_MsgPointer ptr = *p;
		if (!hostMatchesFileEndianness) {
			ptr.name_offset	= __swab32(ptr.name_offset);
			ptr.text_offset		= __swab32(ptr.text_offset);
			ptr.placeholder_offset	= __swab32(ptr.placeholder_offset);
		}

		const char *const pMsgName = reinterpret_cast<const char*>(&pOffTblU8[ptr.name_offset]);
		// TODO: Verify alignment.
		const char16_t *pMsgText = reinterpret_cast<const char16_t*>(&pOffTblU8[ptr.text_offset]);

		const char *const pPlaceholderName = (ptr.placeholder_offset != 0
			? reinterpret_cast<const char*>(&pOffTblU8[ptr.placeholder_offset])
			: nullptr);

		if (pMsgName >= reinterpret_cast<const char*>(pOffTblEndU8)) {
			// MsgName is out of range.
			// TODO: Store more comprehensive error information.
			break;
		} else if (pMsgText >= reinterpret_cast<const char16_t*>(pOffTblEndU8)) {
			// MsgText is out of range.
			// TODO: Store more comprehensive error information.
			break;
		} else if (pPlaceholderName && pPlaceholderName >= reinterpret_cast<const char*>(pOffTblEndU8)) {
			// PlaceholderName is out of range.
			// TODO: Store more comprehensive error information.
			break;
		}

		// Get the message name.
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pOffTblEndU8) - pMsgName);
		string msgName = cpN_to_utf8(932, pMsgName, static_cast<int>(msgNameLen));

		// Find the end of the message text.
		size_t len = 0;
		if (m_isBigEndian) {
			for (; pMsgText < reinterpret_cast<const char16_t*>(pOffTblEndU8); pMsgText++) {
				if (*pMsgText == cpu_to_be16(0)) {
					// Found the NULL terminator.
					break;
				}
				msgText += static_cast<char16_t>(be16_to_cpu(*pMsgText));
			}
		} else {
			for (; pMsgText < reinterpret_cast<const char16_t*>(pOffTblEndU8); pMsgText++) {
				if (*pMsgText == cpu_to_le16(0)) {
					// Found the NULL terminator.
					break;
				}
				msgText += static_cast<char16_t>(le16_to_cpu(*pMsgText));
			}
		}

		// Save the string table entry.
		// NOTE: Saving entries for empty strings, too.
		m_vStrTbl.emplace_back(std::make_pair(msgName, std::move(msgText)));
		m_vStrLkup.insert(std::make_pair(std::move(msgName), idx));

		// Get the placeholder name, if specified.
		if (pPlaceholderName) {
			size_t placeholderNameLen = strnlen(pPlaceholderName, reinterpret_cast<const char*>(pOffTblEndU8) - pPlaceholderName);
			string placeholderName = cpN_to_utf8(932, pPlaceholderName, static_cast<int>(placeholderNameLen));
			m_mapPlaceholder.insert(std::make_pair(idx, std::move(placeholderName)));
		}
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
	m_mapPlaceholder.clear();
	m_vStrLkup.clear();
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

		// Placeholder name, if any.
		const char *const placeholder_name = xml_msg->Attribute("placeholder");
		if (placeholder_name) {
			m_mapPlaceholder.insert(std::make_pair(index, placeholder_name));
		}
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
	vector<WTXT_MsgPointer> vOffsetTbl;	// Primary offset table.
						// NOTE: If any field contains INVALID_OFFSET,
						// it should be written as an actual zero.
	vector<char16_t> vMsgText;	// Message text.
	vector<char> vMsgNames;		// Message names.

	// Differential offset table.
	// This usually consists of 'AB' for strings with names and text,
	// or 'AAA' for strings with names, text, and placeholders.
	vector<uint8_t> vDiffOffTbl;

	// String deduplication for vMsgNames.
	// TODO: Do we need to deduplicate *all* strings, or just the string table name.
	unordered_map<string, size_t> map_nameDedupe;

	// TODO: Better size reservations.
	vOffsetTbl.reserve(m_vStrTbl.size());
	vMsgText.reserve(m_vStrTbl.size() * 32);
	vMsgNames.reserve(m_vStrTbl.size() * 32);
	vDiffOffTbl.reserve(((((m_vStrTbl.size() * 2) + m_mapPlaceholder.size())) + 3) & ~(size_t)(3U));

	// String table name.
	// NOTE: While this is part of the names table, the offset is stored
	// in the WTXT header, *not* the offset table.
	{
		if (!m_name.empty()) {
			const size_t name_size = m_name.size();
			// +1 for NULL terminator.
			vMsgNames.resize(name_size + 1);
			memcpy(vMsgNames.data(), m_name.c_str(), name_size+1);

			// Add to the name deduplication map.
			map_nameDedupe.insert(std::make_pair(m_name, 0));
		} else {
			// Empty string table name...
			// TODO: Report a warning.
			const char empty_name[] = "mst06_generic_name";
			vMsgNames.resize(sizeof(empty_name));
			memcpy(vMsgNames.data(), empty_name, sizeof(empty_name));
		}

		// Differential offset table initialization:
		// - 'A': Skip "WTXT"
		// - 'B': Skip string table name offset and count.
		vDiffOffTbl.push_back('A');
		vDiffOffTbl.push_back('B');
	}

	// Host endianness.
	static const bool hostIsBigEndian = (SYS_BYTEORDER == SYS_BIG_ENDIAN);
	const bool hostMatchesFileEndianness = (hostIsBigEndian == m_isBigEndian);

	size_t idx = 0;
	for (auto iter = m_vStrTbl.cbegin(); iter != m_vStrTbl.cend(); ++iter, ++idx) {
		WTXT_MsgPointer ptr;
		ptr.name_offset = INVALID_OFFSET;
		ptr.text_offset = INVALID_OFFSET;
		ptr.placeholder_offset = INVALID_OFFSET;

		// Copy the message name.
		if (!iter->first.empty()) {
			// Is the name already present?
			// This usually occurs if a string has the same name as the string table.
			// TODO: Do we need to deduplicate *all* strings, or just the string table name.
			auto map_iter = map_nameDedupe.find(iter->first);
			if (map_iter != map_nameDedupe.end()) {
				// Found the string.
				ptr.name_offset = static_cast<uint32_t>(map_iter->second);
			} else {
				// String not found, so cannot dedupe.
				ptr.name_offset = static_cast<uint32_t>(vMsgNames.size());

				// Convert to Shift-JIS first.
				// TODO: Show warnings for strings with characters that
				// can't be converted to Shift-JIS?
				const string sjis_str = utf8_to_cpN(932, iter->first.data(), (int)iter->first.size());
				// Copy the message name into the vector.
				const size_t name_size = sjis_str.size();
				// +1 for NULL terminator.
				vMsgNames.resize(ptr.name_offset + name_size + 1);
				memcpy(&vMsgNames[ptr.name_offset], sjis_str.c_str(), name_size+1);

				// Add the string to the deduplication map.
				map_nameDedupe.insert(std::make_pair(iter->first, ptr.name_offset));
			}
		} else {
			// Empty message name...
			// TODO: Report a warning.
			char buf[64];
			int len = snprintf(buf, sizeof(buf), "XXX_MSG_%zu", idx);
			// +1 for NULL terminator.
			vMsgNames.resize(ptr.name_offset + len + 1);
			memcpy(&vMsgNames[ptr.name_offset], buf, len+1);
		}
		assert(ptr.name_offset <= 16U*1024*1024);

		// Copy the message text.
		// TODO: Add support for writing little-endian files?
		if (!iter->second.empty()) {
			// Copy the message text into the vector.
			// NOTE: c16pos is in units of char16_t, whereas
			// ptr.text_offset is in bytes.
			const uint32_t c16pos = static_cast<uint32_t>(vMsgText.size());
			ptr.text_offset = c16pos * sizeof(char16_t);
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
			vMsgText.resize(c16pos + msg_size + 1);
			// +1 for NULL terminator.
			memcpy(&vMsgText[c16pos], msg_text.c_str(), (msg_size+1) * sizeof(char16_t));

			// NOTE: Text offset must be multiplied by sizeof(char16_t),
			// since vMsgText is char16_t, but vOffsetTbl uses bytes.
			assert(ptr.text_offset <= 16U*1024*1024);
		}

		// Do we have a placeholder name?
		auto plc_iter = m_mapPlaceholder.find(idx);
		if (plc_iter != m_mapPlaceholder.end()) {
			// Is the name already present?
			// This usually occurs if a string has the same name as the string table.
			// TODO: Do we need to deduplicate *all* strings, or just the string table name.
			auto map_iter = map_nameDedupe.find(plc_iter->second);
			if (map_iter != map_nameDedupe.end()) {
				// Found the string.
				ptr.placeholder_offset = static_cast<uint32_t>(map_iter->second);
			} else {
				// String not found, so cannot dedupe.
				ptr.placeholder_offset = static_cast<uint32_t>(vMsgNames.size());

				// Convert to Shift-JIS first.
				// TODO: Show warnings for strings with characters that
				// can't be converted to Shift-JIS?
				const string sjis_str = utf8_to_cpN(932, plc_iter->second.data(), (int)plc_iter->second.size());
				// Copy the message name into the vector.
				const size_t name_size = sjis_str.size();
				// +1 for NULL terminator.
				vMsgNames.resize(ptr.placeholder_offset + name_size + 1);
				memcpy(&vMsgNames[ptr.placeholder_offset], sjis_str.c_str(), name_size+1);

				// Add the string to the deduplication map.
				map_nameDedupe.insert(std::make_pair(plc_iter->second, ptr.placeholder_offset));
			}
		}

		// Add differential offset values.
		assert(ptr.name_offset != INVALID_OFFSET);
		assert(ptr.text_offset != INVALID_OFFSET);
		if (ptr.name_offset != INVALID_OFFSET && ptr.text_offset != INVALID_OFFSET) {
			if (ptr.placeholder_offset != INVALID_OFFSET) {
				// Placeholder name is present.
				vDiffOffTbl.push_back('A');
				vDiffOffTbl.push_back('A');
				vDiffOffTbl.push_back('A');
			} else {
				// Placeholder name is NOT present.
				vDiffOffTbl.push_back('A');
				vDiffOffTbl.push_back('B');
			}
		} else {
			// ERROR: Name and text must be present...
			// TODO: More comprehensive error reporting.
			return -EIO;
		}

		// Add the offsets to the offset table.
		vOffsetTbl.push_back(ptr);
	}

	// Remove the last differential offset table entry,
	// since it's EOF.
	assert(!vDiffOffTbl.empty());
	if (!vDiffOffTbl.empty()) {
		vDiffOffTbl.resize(vDiffOffTbl.size()-1);
	}

	// Determine the message table base addresses.
	const uint32_t text_tbl_base = static_cast<uint32_t>(sizeof(wtxt_header) + (vOffsetTbl.size() * sizeof(WTXT_MsgPointer)));
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
	const uint32_t msg_tbl_count = static_cast<uint32_t>(vOffsetTbl.size());
	wtxt_header.msg_tbl_count = cpu_to_be32(msg_tbl_count);

	// Update the offset table base addresses.
	for (auto iter = vOffsetTbl.begin(); iter != vOffsetTbl.end(); ++iter) {
		if (iter->name_offset == INVALID_OFFSET) {
			iter->name_offset = 0;
		} else {
			iter->name_offset += name_tbl_base;
		}

		if (iter->text_offset == INVALID_OFFSET) {
			iter->text_offset = 0;
		} else {
			iter->text_offset += text_tbl_base;
		}

		if (iter->placeholder_offset == INVALID_OFFSET) {
			iter->placeholder_offset = 0;
		} else {
			iter->placeholder_offset += name_tbl_base;
		}

		if (!hostMatchesFileEndianness) {
			// Byteswap the offsets.
			iter->name_offset		= __swab32(iter->name_offset);
			iter->text_offset		= __swab32(iter->text_offset);
			iter->placeholder_offset	= __swab32(iter->placeholder_offset);
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
	size = fwrite(vOffsetTbl.data(), sizeof(WTXT_MsgPointer), vOffsetTbl.size(), fp);
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

	size_t idx = 0;
	for (auto iter = m_vStrTbl.cbegin(); iter != m_vStrTbl.cend(); ++iter, ++idx) {
		XMLElement *const xml_msg = xml.NewElement("message");
		xml_mst06->InsertEndChild(xml_msg);
		xml_msg->SetAttribute("index", static_cast<unsigned int>(idx));
		xml_msg->SetAttribute("name", iter->first.c_str());

		if (!iter->second.empty()) {
			xml_msg->SetText(escape(utf16_to_utf8(iter->second)).c_str());
		}

		// Is there placeholder text?
		auto plc_iter = m_mapPlaceholder.find(idx);
		if (plc_iter != m_mapPlaceholder.end()) {
			// Save the placeholder text as an attribute.
			xml_msg->SetAttribute("placeholder", escape(plc_iter->second).c_str());
		}
	}

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
		printf("%s\n", escape(utf16_to_utf8(iter->second)).c_str());

		// Is there a placeholder name associated with this message?
		auto plc_iter = m_mapPlaceholder.find(idx);
		if (plc_iter != m_mapPlaceholder.end()) {
			printf("*** Placeholder: %s\n", plc_iter->second.c_str());
		}
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
	bool isSpaceOnly = true;

	string ret;
	ret.reserve(str.size() + 8);
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		switch (*iter) {
			case '\\':
				ret += "\\\\";
				isSpaceOnly = false;
				break;
			case '\n':
				ret += "\\n";
				isSpaceOnly = false;
				break;
			case '\f':
				ret += "\\f";
				isSpaceOnly = false;
				break;
			default:
				ret += *iter;
				if (*iter != ' ') {
					isSpaceOnly = false;
				}
				break;
		}
	}

	// If the text is *only* spaces, change the first space to
	// "\x20" to work around a bug in TinyXML2 where the text
	// is assumed to be completely empty.
	if (!ret.empty() && isSpaceOnly) {
		ret = string("\\x20") + ret.substr(1);
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
	bool isSpaceOnly = true;

	u16string ret;
	ret.reserve(str.size() + 8);
	for (auto iter = str.cbegin(); iter != str.cend(); ++iter) {
		switch (*iter) {
			case '\\':
				ret += u"\\\\";
				isSpaceOnly = false;
				break;
			case '\n':
				ret += u"\\n";
				isSpaceOnly = false;
				break;
			case '\f':
				ret += u"\\f";
				isSpaceOnly = false;
				break;
			default:
				ret += *iter;
				if (*iter != u' ') {
					isSpaceOnly = false;
				}
				break;
		}
	}

	// If the text is *only* spaces, change the first space to
	// "\x20" to work around a bug in TinyXML2 where the text
	// is assumed to be completely empty.
	if (!ret.empty() && isSpaceOnly) {
		ret = u16string(u"\\x20") + ret.substr(1);
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
	for (const char *p = str.c_str(); *p != '\0'; p++) {
		if (*p != '\\') {
			// Not an escape character.
			ret += *p;
			continue;
		}

		// Escape character.
		p++;
		if (*p == '\0') {
			// Backslash at the end of the string.
			ret += '\\';
			break;
		}
		switch (*p) {
			case '\\':
				ret += '\\';
				break;
			case 'n':
				ret += '\n';
				break;
			case 'f':
				ret += '\f';
				break;
			case 'x':
				// Next two characters must be hexadecimal digits.
				if (!isxdigit(p[1]) || !isxdigit(p[2])) {
					// Invalid sequence.
					// Skip over the "\\x" and continue.
					// TODO: Return an error?
				} else {
					// Valid sequence. Convert to uint8_t.
					char buf[3];
					buf[0] = p[1];
					buf[1] = p[2];
					buf[2] = 0;
					char chr = static_cast<char>(strtoul(buf, nullptr, 16));
					ret += chr;
					p += 2;
				}
				break;
			default:
				// Invalid escape sequence.
				ret += '\\';
				ret += *p;
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
	for (const char16_t *p = str.c_str(); *p != u'\0'; p++) {
		if (*p != u'\\') {
			// Not an escape character.
			ret += *p;
			continue;
		}

		// Escape character.
		p++;
		if (*p == u'\0') {
			// Backslash at the end of the string.
			ret += u'\\';
			break;
		}
		switch (*p) {
			case '\\':
				ret += u'\\';
				break;
			case 'n':
				ret += u'\n';
				break;
			case 'f':
				ret += u'\f';
				break;
			case 'x':
				// Next two characters must be hexadecimal digits.
				if (p[1] >= 0x0100 || !isxdigit(p[1]) ||
				    p[2] >= 0x0100 || !isxdigit(p[2]))
				{
					// Invalid sequence.
					// Skip over the "\\x" and continue.
					// TODO: Return an error?
				} else {
					// Valid sequence. Convert to uint8_t.
					char buf[3];
					buf[0] = static_cast<char>(p[1]);
					buf[1] = static_cast<char>(p[2]);
					buf[2] = static_cast<char>(0);
					char16_t chr = static_cast<char16_t>(strtoul(buf, nullptr, 16));
					ret += chr;
					p += 2;
				}
				break;
			default:
				// Invalid escape sequence.
				ret += u'\\';
				ret += *p;
				break;
		}
	}
	return ret;
}
