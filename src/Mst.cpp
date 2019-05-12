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

// TODO: Check ENABLE_XML?
#include <tinyxml2.h>
using namespace tinyxml2;

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
	if (!filename || !filename[0]) {
		return -EINVAL;
	}

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
		mst.doff_tbl_offset	= be32_to_cpu(mst.doff_tbl_offset);
		mst.doff_tbl_length	= be32_to_cpu(mst.doff_tbl_length);
	} else {
		mst.file_size		= le32_to_cpu(mst.file_size);
		mst.doff_tbl_offset	= le32_to_cpu(mst.doff_tbl_offset);
		mst.doff_tbl_length	= le32_to_cpu(mst.doff_tbl_length);
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
	if ((uint64_t)sizeof(MST_Header) + (uint64_t)mst.doff_tbl_offset + (uint64_t)mst.doff_tbl_length > mst.file_size) {
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
	const uint8_t *pDOffTbl = &mst_data[sizeof(mst) + mst.doff_tbl_offset];
	const uint8_t *const pDOffTblEnd = pDOffTbl + mst.doff_tbl_length;

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
	for (; pDOffTbl < pDOffTblEnd; pDOffTbl++) {
		// High two bits of this byte indicate how long the offset is.
		uint32_t offset_diff = 0;
		switch (*pDOffTbl >> 6) {
			case 0:
				// 0 bits long. End of offset table.
				doneOffsets = true;
				break;
			case 1:
				// 6 bits long.
				// Take low 6 bits of this byte and left-shift by 2.
				offset_diff = (pDOffTbl[0] & 0x3F) << 2;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 2:
				// 14 bits long.
				// Offset difference is stored in 2 bytes.
				if (pDOffTbl + 2 >= pDOffTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pDOffTbl[0] & 0x3F) << 10) |
				               (pDOffTbl[1] << 2);
				pDOffTbl++;
				break;

			// TODO: Verify this. ('06 doesn't use this; Forces might.)
			case 3:
				// 30 bits long.
				// Offset difference is stored in 4 bytes.
				if (pDOffTbl + 4 >= pDOffTblEnd) {
					// Out of bounds!
					// TODO: Store more comprehensive error information.
					doneOffsets = true;
					break;
				}
				offset_diff = ((pDOffTbl[0] & 0x3F) << 26) |
				               (pDOffTbl[1] << 18) |
				               (pDOffTbl[2] << 10) |
				               (pDOffTbl[3] << 2);
				pDOffTbl += 3;
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

		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pDiffTblEnd) - pMsgName);
		m_name = cpN_to_utf8(932, pMsgName, static_cast<int>(msgNameLen));
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
		size_t msgNameLen = strnlen(pMsgName, reinterpret_cast<const char*>(pDiffTblEnd) - pMsgName);
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

	// Clear the current string tables.
	m_name.clear();
	m_vStrTbl.clear();
	m_vStrLkup.clear();

	// Open the XML document.
	FILE *f_xml = _tfopen(filename, _T("r"));
	if (!f_xml) {
		// Error opening the XML document.
		return -errno;
	}

	// Parse the XML document.
	XMLDocument xml;
	int ret = xml.LoadFile(f_xml);
	fclose(f_xml);
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
		m_vStrTbl[index].second = utf8_to_utf16(msg_text, strlen(msg_text));

		// Add the message to the lookup table.
		m_vStrLkup.insert(std::make_pair(msg_name, index));
	}

	// TODO: Check for missing message indexes.

	// Document processed.
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

	// Create an XML document.
	XMLDocument xml;
	XMLDeclaration *const xml_decl = xml.NewDeclaration();
	xml.InsertFirstChild(xml_decl);
	XMLElement *const xml_mst06 = xml.NewElement("mst06");
	xml.InsertEndChild(xml_mst06);
	xml_mst06->SetAttribute("name", m_name.c_str());

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

	// Save the XML document.
	// NOTE: Using our custom XMLPrinter for tabs instead of spaces.
	FILE *f_xml = _tfopen(filename, _T("w"));
	if (!f_xml) {
		// Error opening the XML file.
		return -errno;
	}

	MstXMLPrinter stream(f_xml, false);
	xml.Print(&stream);
	fclose(f_xml);
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
