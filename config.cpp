#include <stdlib.h>
#include <limits.h>

#include <iostream>
#include <fstream>

#include "main.h"

static void
clearpath(std::string &path)
{
	// mhh, could strip whitespace at the end and allow
	// backslash escaping
	// or we just say screw you to people who use trailing
	// whitespaces in their configs...


	if (path.length() < 2)
		return;
	if (path[0] == '~' && path[1] == '/') {
		path.erase(0, 1);
		path.insert(0, getenv("HOME"));
	}
}

static bool
cfg_database(std::string &line)
{
	clearpath(line);
	opt_default_db = line;
	return true;
}

static bool
cfg_verbosity(std::string &line)
{
	opt_verbosity = strtoul(line.c_str(), nullptr, 0);
	return true;
}

static bool
cfg_quiet(std::string &line)
{
	size_t n = line.find_first_of(" \t\r\n");
	if (n != std::string::npos)
		line = std::move(line.substr(0, n));
	opt_quiet = (line == "true" ||
	             line == "TRUE" ||
	             line == "True" ||
	             line == "on"   ||
	             line == "On"   ||
	             line == "ON"   ||
	             line == "1");
	return true;
}

bool
ReadConfig(std::istream &in, const char *path)
{
	std::string line;

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

		std::tuple<std::string, std::function<bool(std::string&)>>
		rules[] = {
			std::make_tuple("database",  cfg_database),
			std::make_tuple("verbosity", cfg_verbosity),
			std::make_tuple("quiet",     cfg_quiet)
		};

		for (auto &r : rules) {
			std::string &name(std::get<0>(r));
			auto         fn = std::get<1>(r);
			if (line.compare(start, name.length(), name) == 0) {
				size_t eq = line.find_first_not_of(" \t\n\r", start+name.length());
				if (eq == std::string::npos) {
					log(Warn, "%s:%lu: invalid config entry\n", path, (unsigned long)lineno);
					break;
				}
				if (line[eq] != '=') {
					log(Warn, "%s:%lu: missing `=` in config entry\n", path, (unsigned long)lineno);
					break;
				}
				start = line.find_first_not_of(" \t\n\r", eq+1);
				line = std::move(line.substr(start));
				if (!fn(line))
					return false;
				break;
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
