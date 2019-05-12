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

#ifndef __MST06_MST_HPP__
#define __MST06_MST_HPP__

#include "tcharx.h"

#include <string>
#include <unordered_map>
#include <vector>

class Mst
{
	public:
		Mst();
		~Mst() { }

	public:
		// Disable copying.
		Mst(const Mst&) = delete;
		Mst &operator=(const Mst&) = delete;

	public:
		/**
		 * Load an MST string table.
		 * @param filename MST string table filename.
		 * @return 0 on success; negative POSIX error code on error.
		 */
		int loadMST(const TCHAR *filename);

		/**
		 * Load an XML string table.
		 * @param filename	[in] XML filename.
		 * @param pVecErrs	[out,opt] Vector of user-readable error messages.
		 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
		 */
		int loadXML(const TCHAR *filename, std::vector<std::string> *pVecErrs = nullptr);

		/**
		 * Save the string table as XML.
		 * @param filename XML filename.
		 * @return 0 on success; negative POSIX error code or positive TinyXML2 error code on error.
		 */
		int saveXML(const TCHAR *filename) const;

	public:
		// TODO: Save MST, Load XML
		// TODO: Iterator and mutator functions.

	public:
		/**
		 * Dump the string table to stdout.
		 */
		void dump(void) const;

	public:
		/** Accessors **/

		/**
		 * Is the file big-endian?
		 * @return True if the file is big-endian; false if not.
		 */
		bool isBigEndian(void) const
		{
			return m_isBigEndian;
		}

		/**
		 * Get the string table name.
		 * @return String table name.
		 */
		const std::string &tblName(void) const
		{
			return m_name;
		}

		/**
		 * Get the string count.
		 * @return Number of strings.
		 */
		size_t strCount(void) const
		{
			return m_vStrTbl.size();
		}

		/**
		 * Get a string's name. (UTF-8)
		 * @param idx String index.
		 * @return String name. (UTF-8)
		 */
		std::string strName(size_t index) const
		{
			if (index >= m_vStrTbl.size())
				return std::string();
			return m_vStrTbl[index].first;
		}

		/**
		 * Get a string's text. (UTF-8)
		 * @param idx String index.
		 * @return String text. (UTF-8)
		 */
		std::string strText_utf8(size_t index);

		/**
		 * Get a string's text. (UTF-8)
		 * @param idx String name. (UTF-8)
		 * @return String text. (UTF-8)
		 */
		std::string strText_utf8(const std::string &name);

		/**
		 * Get a string's text. (UTF-16)
		 * @param idx String index.
		 * @return String text. (UTF-16)
		 */
		std::u16string strText_utf16(size_t index);

		/**
		 * Get a string's text. (UTF-16)
		 * @param idx String name. (UTF-8)
		 * @return String text. (UTF-16)
		 */
		std::u16string strText_utf16(const std::string &name);

	public:
		/** String escape functions **/
		// TODO: Use templates?

		/**
		 * Escape a UTF-8 string.
		 * @param str Unescaped string.
		 * @return Escaped string.
		 */
		static std::string escape(const std::string &str);

		/**
		 * Escape a UTF-16 string.
		 * @param str Unescaped string.
		 * @return Escaped string.
		 */
		static std::u16string escape(const std::u16string &str);

		/**
		 * Unescape a UTF-8 string.
		 * @param str Escaped string.
		 * @return Unescaped string.
		 */
		static std::string unescape(const std::string &str);

		/**
		 * Unescape a UTF-16 string.
		 * @param str Escaped string.
		 * @return Unscaped string.
		 */
		static std::u16string unescape(const std::u16string &str);

	private:
		// MST information.
		uint8_t m_version;	// MST version number.
		bool m_isBigEndian;	// True if this file is big-endian.

		// String table name. (UTF-8)
		std::string m_name;
		
		// Main string table.
		// - Index: String index.
		// - First: String name. (UTF-8)
		// - Second: String text. (UTF-16)
		std::vector<std::pair<std::string, std::u16string> > m_vStrTbl;

		// String name to index lookup.
		// - Key: String name. (UTF-8)
		// - Value: String index.
		std::unordered_map<std::string, size_t> m_vStrLkup;
};

#endif /* __MST06_MST_HPP__ */
