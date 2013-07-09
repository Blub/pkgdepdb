#include <stdlib.h>
#include <limits.h>

#include <fstream>
#include <regex>

#include "main.h"

static void
clearpath(std::string &path)
{
	(void)path;
	// mhh, could strip whitespace at the end and allow
	// backslash escaping
	// or we just say screw you to people who use trailing
	// whitespaces in their configs...
}


// NOTE:  I use std::regex because I want to test it out
// FIXME: this could be as simple as sscanf :P

bool
ReadConfig(std::istream &in, const char *path)
{
	std::string line;
	std::regex entry("\\s*(database|verbosity|quiet)\\s*=\\s*(.*$)",
	                std::regex_constants::basic);

	std::smatch match;
	size_t lineno = 0;
	while (std::getline(in, line)) {
		++lineno;
		size_t start = line.find_first_not_of(" \t\n\r");
		// no content
		if (start == std::string::npos)
			continue;
		// comment lines
		if (line[start] == '#' ||
		    line[start] == '/' ||
		    line[start] == ';')
		{
			continue;
		}

		if (!std::regex_match(line, match, entry)) {
			log(Warn, "%s:%lu: invalid config entry\n", path, (unsigned long)lineno);
			continue;
		}

		std::ssub_match etype = match[0];
		if (etype.str() == "database") {
			if (match.size() != 2)
				opt_default_db = "";
			else {
				std::string file(std::move(match[1].str()));
                clearpath(file);
                opt_default_db = file;
			}
		}
		else if (etype.str() == "verbosity") {
			if (match.size() != 2)
				opt_verbosity = 0;
			else
				opt_verbosity = strtoul(match[1].str().c_str(), nullptr, 0);
		}
		else if (etype.str() == "quiet") {
			if (match.size() != 2)
				opt_quiet = false;
			else {
				std::string what(std::move(match[1].str()));
				opt_quiet = (what == "true" ||
				             what == "TRUE" ||
				             what == "True" ||
				             what == "1");
			}
		}
	}
	return true;
}

bool
ReadConfig()
{
	std::string home(getenv("HOME"));
	std::string etcdir(PKGDEPDB_ETC);

	std::vector<std::string> config_paths({
		home   + "/.pkgdepdb/config",
		etcdir + "/pkgdepdb.conf"
	});

	for (auto &p : config_paths) {
		std::ifstream in(p);
		if (!in)
			continue;
		return ReadConfig(in, p.c_str());
	}
	// no config found, that's okay
	return true;
}
