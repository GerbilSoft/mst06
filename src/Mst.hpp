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
		Mst() { }
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
		 * Dump the string table to stdout.
		 */
		void dump(void) const;

	public:
		// TODO: Save MST, Load XML, Save XML
		// TODO: Iterator and mutator functions.

	public:
		/** Accessor functions. **/

		/**
		 * Get the string table name.
		 * @return String table name.
		 */
		const std::string &tblName(void) const
		{
			return m_name;
		}

		// TODO: More accessors.

	private:
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
