/***************************************************************************
 * MST Decoder/Encoder for Sonic '06                                       *
 * main.cpp: Main program.                                                 *
 *                                                                         *
 * Copyright (c) 2019 by David Korth.                                      *
 * SPDX-License-Identifier: GPL-3.0-or-later                               *
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
	if (argc != 2 && argc != 3) {
		_ftprintf(stderr, _T("Syntax: %s [filenames]\n\n"), argv[0]);
		_ftprintf(stderr, _T("- Convert MST to XML: %s mst_file.mst [mst_file.xml]\n"), argv[0]);
		_ftprintf(stderr, _T("- Convert XML to MST: %s mst_file.xml [mst_file.mst]\n\n"), argv[0]);
		_ftprintf(stderr, _T("Default output filename replaces the file extension on the\n"));
		_ftprintf(stderr, _T("input file with .xml or .mst, depending on operation.\n"));
		return EXIT_FAILURE;
	}

	// Open the file and check if it's MST or XML.
	FILE *f_in = _tfopen(argv[1], _T("rb"));
	if (!f_in) {
		_ftprintf(stderr, _T("*** ERROR opening %s: %s\n"), argv[1], _tcserror(errno));
		return EXIT_FAILURE;
	}

	char buf[32];
	errno = 0;
	size_t size = fread(buf, 1, sizeof(buf), f_in);
	int err = errno;
	if (size != sizeof(buf)) {
		if (err == 0) err = EIO;
		_ftprintf(stderr, _T("*** ERROR reading file %s: %s\n"), argv[1], _tcserror(errno));
		return EXIT_FAILURE;
	}
	rewind(f_in);

	Mst mst;
	int ret;

	// Check if this is XML.
	const TCHAR *out_ext = nullptr;
	bool writeXML = false, writeMST = false;
	if (!memcmp(buf, "<?xml ", 6)) {
		// This is an XML file.
		// Parse as XML and convert to MST.
		out_ext = _T(".mst");
		writeMST = true;
		// TODO: Print load errors.
		ret = mst.loadXML(f_in);
		fclose(f_in);
	} else if (!memcmp(&buf[0x18], "BINA", 4)) {
		// This is an MST file.
		// Parse as MST and convert to XML.
		out_ext = _T(".xml");
		writeXML = true;
		ret = mst.loadMST(f_in);
		fclose(f_in);
	} else {
		// Unrecognized file format.
		fclose(f_in);
		_ftprintf(stderr, _T("*** ERROR: File %s is not recognized.\n"), argv[1]);
		return EXIT_FAILURE;
	}

	if (ret != 0) {
		_ftprintf(stderr, _T("*** ERROR loading %s: %s\n"), argv[1], _tcserror(-ret));
		return EXIT_FAILURE;
	}

	tstring out_filename;
	if (argc == 2) {
		// Output filename not specified.
		// Create the filename.
		// NOTE: If it's an absolute path, the XML file will be
		// stored in the same directory as the MST file.
		out_filename = argv[1];
		bool replaced_ext = false;
		size_t slashpos = out_filename.rfind(SLASH_CHAR);
		size_t dotpos = out_filename.rfind(_T('.'));
		if (dotpos != tstring::npos) {
			// We have a dot.
			// If a slash is present, it must be before the dot.
			if (slashpos == tstring::npos || slashpos < dotpos) {
				// Replace the extension.
				out_filename.resize(dotpos);
				out_filename += out_ext;
				replaced_ext = true;
			}
		}

		if (!replaced_ext) {
			// No extension to replace.
			// Add an extension.
			out_filename += out_ext;
		}
	} else /*if (argc == 3)*/ {
		// Output filename is specified.
		out_filename = argv[2];
	}

	ret = 0;
	if (writeXML) {
		// Convert to XML.
		ret = mst.saveXML(out_filename.c_str());
		_tprintf(_T("*** saveXML to %s: %d\n"), out_filename.c_str(), ret);
	} else if (writeMST) {
		// Convert to MST.
		ret = mst.saveMST(out_filename.c_str());
		_tprintf(_T("*** saveMST to %s: %d\n"), out_filename.c_str(), ret);
	}
	return ret;
}
