/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * main.cpp: Main program.                                                 *
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
using std::pair;
using std::string;
using std::vector;
using std::wstring;

#include "tcharx.h"
#include "Mst.hpp"

#ifdef _WIN32
# define SLASH_CHAR _T('\\')
#else /* !_WIN32 */
# define SLASH_CHAR '/'
#endif

int _tmain(int argc, TCHAR *argv[])
{
	if (argc != 2) {
		_ftprintf(stderr, _T("Syntax: %s mst_file.mst\n"), argv[0]);
		_ftprintf(stderr, _T("File will be converted to mst_file.xml.\n"));
		return EXIT_FAILURE;
	}

	Mst mst;
	int ret = mst.loadMST(argv[1]);
	if (ret != 0) {
		_ftprintf(stderr, _T("*** ERROR loading %s: %s\n"), argv[1], _tcserror(-ret));
		return EXIT_FAILURE;
	}

	// Create a filename for the XML file.
	// NOTE: If it's an absolute path, the XML file will be
	// stored in the same directory as the MST file.
	tstring xml_filename(argv[1]);
	bool replaced_ext = false;
	size_t slashpos = xml_filename.rfind(SLASH_CHAR);
	size_t dotpos = xml_filename.rfind(_T('.'));
	if (dotpos != tstring::npos) {
		// We have a dot.
		// If a slash is present, it must be before the dot.
		if (slashpos == tstring::npos || slashpos < dotpos) {
			// Replace the extension.
			xml_filename.resize(dotpos);
			xml_filename += _T(".xml");
			replaced_ext = true;
		}
	}

	if (!replaced_ext) {
		// No extension to replace.
		// Add an extension.
		xml_filename += _T(".xml");
	}

	// Convert to XML.
	ret = mst.saveXML(xml_filename.c_str());
	_tprintf(_T("*** saveXML to %s: %d\n"), xml_filename.c_str(), ret);
	return ret;
}
