/*************************************************************************
** CmdLineParserBase.cpp                                                **
**                                                                      **
** This file is part of dvisvgm -- the DVI to SVG converter             **
** Copyright (C) 2005-2010 Martin Gieseking <martin.gieseking@uos.de>   **
**                                                                      **
** This program is free software; you can redistribute it and/or        **
** modify it under the terms of the GNU General Public License as       **
** published by the Free Software Foundation; either version 3 of       **
** the License, or (at your option) any later version.                  **
**                                                                      **
** This program is distributed in the hope that it will be useful, but  **
** WITHOUT ANY WARRANTY; without even the implied warranty of           **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         **
** GNU General Public License for more details.                         **
**                                                                      **
** You should have received a copy of the GNU General Public License    **
** along with this program; if not, see <http://www.gnu.org/licenses/>. **
*************************************************************************/

#include <cstring>
#include <iostream>
#include "CmdLineParserBase.h"
#include "InputBuffer.h"
#include "InputReader.h"
#include "Message.h"

using namespace std;

void CmdLineParserBase::init () {
	_error = false;
	_files.clear();
}

/** Parses all options given on the command line.
 *  @param[in] printErrors enable/disable printing of error messages */
void CmdLineParserBase::parse (int argc, char **argv, bool printErrors) {
	init();
	_printErrors = printErrors;
	bool filesOnly = false;  //
	for (int i=1; i < argc; i++) {
		CharInputBuffer ib(argv[i], strlen(argv[i]));
		BufferInputReader ir(ib);
		if (filesOnly || ir.peek() != '-')
			_files.push_back(argv[i]);
		else {
			ir.get();
			if (ir.peek() == '-') {
				// scan long option
				ir.get();
				if (ir.eof())  // "--" only
					filesOnly = true;  // treat all following options as filenames
				else {
					string longname;
					while (isalnum(ir.peek()) || ir.peek() == '-')
						longname += ir.get();
					if (const Option *opt = option(longname))
						(*opt->handler)(this, ir, *opt, true);
					else if (!_error) {
						if (printErrors)
							Message::estream(false) << "unknown option --" << longname << '\n';
						_error = true;
					}
				}
			}
			else {
				// scan short option(s)
				bool combined = false;  // multiple short options combined, e.g -abc
				do {
					int shortname = ir.get();
					if (const Option *opt = option(shortname)) {
						if (!combined || opt->argmode == 0) {
							if (opt->argmode == 'r' && strlen(argv[i]) == 2) { // required argument separated by whitespace?
								if (i+1 < argc && argv[i+1][0] != '-')
									ib.assign(argv[++i]);
							}
							(*opt->handler)(this, ir, *opt, false);
							if (opt->argmode == 0)
								combined = true;
						}
						else {
							if (printErrors)
								Message::estream(false) << "option -" << char(shortname) << " must be given separately\n";
							_error = true;
						}
					}
					else if (shortname > 0) {
						if (printErrors)
							Message::estream(false) << "unknown option -" << shortname << '\n';
						_error = true;
					}
				}
				while (!_error && combined && !ir.eof());
			}
		}
	}
}


/** Prints an error message to stdout.
 *  @param[in] opt error occurred in this option
 *  @param[in] longopt the long option name was scanned
 *  @param[in] msg message to be printed */
void CmdLineParserBase::error (const Option &opt, bool longopt, const char *msg) const {
	if (_printErrors) {
		Message::estream(false) << "option ";
		if (longopt)
			Message::estream(false) << "--" << opt.longname;
		else
			Message::estream(false) << '-' << opt.shortname;
		Message::estream(false) << ": " << msg << '\n';
	}
	_error = true;
}


/** Lists the scanned filenames. Just for debugging purposes. */
void CmdLineParserBase::status () const {
	cout << "file names:\n";
	for (size_t i=0; i < _files.size(); i++)
		cout << "  " << _files[i] << endl;
	cout << endl;
}


/** Returns the option information of a given short option name.
 *  If the option name can't be found 0 is returned.
 *  @param[in] longname long version of the option without leading hyphen (e.g. p, not -p) */
const CmdLineParserBase::Option* CmdLineParserBase::option (char shortname) const {
	for (const Option *opts = options(); opts->longname; ++opts)
		if (opts->shortname == shortname)
			return opts;
	return 0;
}


/** Returns the option information of a given long option name.
 *  Parameter 'longname' hasn't to be the complete long option name. The function looks up
 *  all options that start with 'longname'. If a unique or an exact match was found, it's returned.
 *  Otherwise, the return value is 0.
 *  @param[in] longname long version of the option without leading hyphens (e.g. param, not --param) */
const CmdLineParserBase::Option* CmdLineParserBase::option (const string &longname) const {
	vector<const Option*> matches;  // all matching options
	size_t len = longname.length();
	for (const Option *opts = options(); opts->longname; ++opts) {
		if (string(opts->longname, len) == longname) {
			if (len == strlen(opts->longname))  // exact match?
				return opts;
			matches.push_back(opts);
		}
	}
	switch (matches.size()) {
		default:
			if (_printErrors) {
				Message::estream(false) << "option --" << longname << " is ambiguous (";
				for (size_t i=0; i < matches.size(); i++) {
					if (i > 0)
						Message::estream(false) << ", ";
					Message::estream(false) << matches[i]->longname;
				}
				Message::estream(false) << ")\n";
			}
			_error = true;

		case 0 : return 0;
		case 1 : return matches[0];
	}
}


/** Returns true if a valid separator between option and argument was found.
 *  Arguments of long options are preceded by a '='. The argument of a short option
 *  directly follows the option without a separation character.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given */
bool CmdLineParserBase::checkArgPrefix (InputReader &ir, const Option &opt, bool longopt) const {
	if (longopt) {
		if (ir.peek() == '=')
			ir.get();
		else {
			error(opt, longopt, "'=' expected");
			return false;
		}
	}
	return true;
}


/** Returns true if a given option has no argument, .e.g. -p or --param.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given */
bool CmdLineParserBase::checkNoArg (InputReader &ir, const Option &opt, bool longopt) const {
	if (ir.eof())
		return true;
	error(opt, longopt, "no argument expected");
	return false;
}


/** Gets an integer argument of a given option, e.g. -p5 or --param=5.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getIntArg (InputReader &ir, const Option &opt, bool longopt, int &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		if (ir.parseInt(arg) && ir.eof())
			return true;
		error(opt, longopt, "integer value expected");
	}
	return false;
}


/** Gets an unsigned integer argument of a given option, e.g. -p5 or --param=5.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getUIntArg (InputReader &ir, const Option &opt, bool longopt, unsigned &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		if (ir.parseUInt(arg) && ir.eof())
			return true;
		error(opt, longopt, "unsigned integer value expected");
	}
	return false;
}


/** Gets a double (floating point) argument of a given option, e.g. -p2.5 or --param=2.5.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getDoubleArg (InputReader &ir, const Option &opt, bool longopt, double &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		if (ir.parseDouble(arg) != 0 && ir.eof())
			return true;
		error(opt, longopt, "floating point value expected");
	}
	return false;
}


/** Gets a string argument of a given option, e.g. -pstr or --param=str.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getStringArg (InputReader &ir, const Option &opt, bool longopt, string &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		arg.clear();
		while (!ir.eof())
			arg += ir.get();
		if (!arg.empty())
			return true;
		error(opt, longopt, "string argument expected");
	}
	return false;
}


/** Gets a boolean argument of a given option, e.g. -pyes or --param=yes.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getBoolArg (InputReader &ir, const Option &opt, bool longopt, bool &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		string str;
		while (!ir.eof())
			str += ir.get();
		if (str == "yes" || str == "y" || str == "true" || str == "1") {
			arg = true;
			return true;
		}
		else if (str == "no" || str == "n" || str == "false" || str == "0") {
			arg = false;
			return true;
		}
		error(opt, longopt, "boolean argument expected (yes, no, true, false, 0, 1)");
	}
	return false;
}


/** Gets a (single) character argument of a given option, e.g. -pc or --param=c.
 *  @param[in]  ir argument is read from this InputReader
 *  @param[in]  opt scans argument of this option
 *  @param[in]  longopt true if the long option name was given
 *  @param[out] arg the scanned option argument
 *  @return true if argument could be scanned without errors */
bool CmdLineParserBase::getCharArg (InputReader &ir, const Option &opt, bool longopt, char &arg) const {
	if (checkArgPrefix(ir, opt, longopt)) {
		arg = ir.get();
		if (arg >= 0 && ir.eof())
			return true;
		error(opt, longopt, "character argument expected");
	}
	return false;
}
