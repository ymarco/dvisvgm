/***********************************************************************
** MetafontWrapper.cpp                                                **
**                                                                    **
** This file is part of dvisvgm -- the DVI to SVG converter           **
** Copyright (C) 2005-2009 Martin Gieseking <martin.gieseking@uos.de> **
**                                                                    **
** This program is free software; you can redistribute it and/or      **
** modify it under the terms of the GNU General Public License        **
** as published by the Free Software Foundation; either version 2     **
** of the License, or (at your option) any later version.             **
**                                                                    **
** This program is distributed in the hope that it will be useful,    **
** but WITHOUT ANY WARRANTY; without even the implied warranty of     **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the      **
** GNU General Public License for more details.                       **
**                                                                    **
** You should have received a copy of the GNU General Public License  **
** along with this program; if not, write to the Free Software        **
** Foundation, Inc., 51 Franklin Street, Fifth Floor,                 **
** Boston, MA 02110-1301, USA.                                        **
***********************************************************************/

#include <cstdlib>
#include <cctype>
#include <fstream>
#include <sstream>
#include "FileSystem.h"
#include "FileFinder.h"
#include "Message.h"
#include "MetafontWrapper.h"
#include "debug.h"

using namespace std;


MetafontWrapper::MetafontWrapper (const string &fname) 
	: _fontname(fname)
{
}

#if 0
/** This helper function tries to find the mf file for a given fontname. */
static const char* lookup (string fontname) {
	// try to find file with exact fontname
	string mfname = fontname+".mf";
	if (const char *path = FileFinder::lookup(mfname))
		return path;
	
	// lookup fontname with trailing numbers stripped
	// (this will find the mf files of ec fonts)
	int pos = fontname.length()-1;
	while (pos >= 0 && isdigit(fontname[pos]))
		pos--;
	mfname = fontname.substr(0, pos+1)+".mf";
	if (const char *path = FileFinder::lookup(mfname))
		return path;
	// not found either => give up
	return 0;
}	
#endif


/** Calls Metafont and evaluates the logfile. If a gf file was successfully
 *  generated the dpi value is stripped from the filename
 *  (e.g. cmr10.600gf => cmr10.gf). This makes life easier...
 *  @param[in] mode Metafont mode, e.g. "ljfour"
 *  @param[in] mag magnification factor
 *  @return return value of Metafont system call */
int MetafontWrapper::call (const string &mode, double mag) {
	if (!FileFinder::lookup(_fontname+".mf"))
		return 1;     // mf file not available => no need to call the "slow" Metafont
	
	FileSystem::remove(_fontname+".gf");	
	ostringstream oss;
	oss << "mf \"\\mode=" << mode  << ";"
		    "mag:=" << mag << ";"
		    "batchmode;"
		    "input " << _fontname << "\" >" << FileSystem::DEVNULL;
	Message::mstream() << "running Metafont for " << _fontname << endl;
	int ret = system(oss.str().c_str());

	// try to read Metafont's logfile and get name of created GF file
	ifstream ifs((_fontname+".log").c_str());
	if (ifs) {	
		char buf[128];
		while (ifs) {		
			ifs.getline(buf, 128);
			string line = buf;
			if (line.substr(0, 17) == "Output written on") {
				size_t pos = line.find("gf ", 18+_fontname.length());
				if (pos != string::npos) {
					string gfname = line.substr(18, pos-16);  // GF filename found
					FileSystem::rename(gfname, _fontname+".gf");
				}
				break;
			}
		}
	}
	return ret;
}


/** Calls Metafont if output files (tfm and gf) don't already exist. */
int MetafontWrapper::make (const string &mode, double mag) {
	ifstream tfm((_fontname+".tfm").c_str());
	ifstream gf((_fontname+".gf").c_str());
	if (gf && tfm) // @@ distinguish between gf and tfm
		return 0;
	return call(mode, mag);
}


bool MetafontWrapper::success () const {
	ifstream tfm((_fontname+".tfm").c_str());
	ifstream gf((_fontname+".gf").c_str());
	return tfm && gf;
}


/** Remove all files created by a Metafont call (tfm, gf, log). 
 *  @param[in] keepGF if true, GF files won't be removed */
void MetafontWrapper::removeOutputFiles (bool keepGF) {
	removeOutputFiles(_fontname, keepGF);
}


/** Remove all files created by a Metafont call for a given font (tfm, gf, log). 
 *  @param[in] fontname name of font whose temporary files should be removed
 *  @param[in] keepGF if true, GF files will be kept */
void MetafontWrapper::removeOutputFiles (const string &fontname, bool keepGF) {
	const char *ext[] = {"gf", "tfm", "log", 0};
	for (const char **p = keepGF ? ext+2 : ext; *p; ++p)
		FileSystem::remove(fontname + "." + *p);
}
