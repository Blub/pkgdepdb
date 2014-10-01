#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <iostream>
#include <fstream>

#include "main.h"
#include "pkgdepdb.h"

namespace pkgdepdb {

Config::Config()
{
}

Config::~Config()
{
}

static void clearpath(string &path) {
  if (path.length() < 2)
    return;
  if (path[0] == '~' && path[1] == '/') {
    const char *home = getenv("HOME");
    if (home) {
      path.erase(0, 1);
      path.insert(0, getenv("HOME"));
    }
  }
}

static function<bool(string&)> cfg_path(string &ref) {
  return [&ref](string &line) -> bool {
    clearpath(line);
    ref = move(line);
    return true;
  };
}

template<typename T>
static function<bool(string&)> cfg_numeric(T &ref) {
  return [&ref](string &line) -> bool {
    ref = static_cast<T>(strtoul(line.c_str(), nullptr, 0));
    return true;
  };
}

static function<bool(string&)> cfg_json(uint &ref) {
  return [&ref](string &line) -> bool {
    return Config::ParseJSONBit(line.c_str(), ref);
  };
}

bool Config::str2bool(const string& line) {
  return (line == "true" ||
          line == "TRUE" ||
          line == "True" ||
          line == "on"   ||
          line == "On"   ||
          line == "ON"   ||
          line == "YES"  ||
          line == "Yes"  ||
          line == "yes"  ||
          line == "1");
}

static bool line_to_bool(string& line) {
  size_t n = line.find_first_of(" \t\r\n");
  if (n != string::npos)
    line = move(line.substr(0, n));
  return Config::str2bool(line);
}

static function<bool(string&)> cfg_bool(bool &ref) {
  return [&ref](string &line) -> bool {
    ref = line_to_bool(line);
    return true;
  };
}

bool Config::ParseJSONBit(const char *bit, uint &opt_json) {
  if (!*bit)
    return true;
  int mode = 0;
  if (bit[0] == '+') {
    mode = '+';
    ++bit;
  }
  else if (bit[0] == '-') {
    mode = '-';
    ++bit;
  }

  if (!strcmp(bit, "a") ||
      !strcmp(bit, "all"))
  {
    if (mode == '-')
      opt_json = 0;
    else
      opt_json = static_cast<uint>(-1);
    return true;
  }

  if (!strcmp(bit, "off") ||
      !strcmp(bit, "n")   ||
      !strcmp(bit, "no")  ||
      !strcmp(bit, "none") )
  {
    if (mode == 0)
      opt_json = 0;
    return true;
  }

  if (!strcmp(bit, "on") ||
      !strcmp(bit, "q")  ||
      !strcmp(bit, "query") )
  {
    if (mode == '+')
      opt_json |= JSONBits::Query;
    else if (mode == '-')
      opt_json &= ~JSONBits::Query;
    else
      opt_json = JSONBits::Query;
    return true;
  }

  if (!strcmp(bit, "db"))
  {
    if (mode == '+')
      opt_json |= JSONBits::DB;
    else if (mode == '-')
      opt_json &= ~JSONBits::DB;
    else
      opt_json = JSONBits::DB;
    return true;
  }

  log(Error, "unknown json bit: %s\n", bit);
  return false;
}

bool Config::ReadConfig(std::istream &in, const char *path) {
  string line;

  size_t lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    size_t start = line.find_first_not_of(" \t\n\r");
    // no content
    if (start == string::npos)
      continue;
    // comment lines
    if (line[start] == '#' ||
        line[start] == '/' ||
        line[start] == ';')
    {
      continue;
    }

    std::tuple<string, function<bool(string&)>>
    rules[] = {
      std::make_tuple("database",         cfg_path(database_)),
      std::make_tuple("verbosity",        cfg_numeric(verbosity_)),
      std::make_tuple("quiet",            cfg_bool(quiet_)),
      std::make_tuple("package_depends",  cfg_bool(package_depends_)),
      std::make_tuple("json",             cfg_json(json_)),
      std::make_tuple("jobs",             cfg_numeric(max_jobs_)),
      std::make_tuple("file_lists",       cfg_bool(package_filelist_)),
    };

    for (auto &r : rules) {
      string &name(std::get<0>(r));
      auto    fn = std::get<1>(r);
      if (line.compare(start, name.length(), name) == 0) {
        size_t eq = line.find_first_not_of(" \t\n\r", start+name.length());
        if (eq == string::npos) {
          log(Warn, "%s:%lu: invalid config entry\n",
              path, (unsigned long)lineno);
          break;
        }
        if (line[eq] != '=') {
          log(Warn, "%s:%lu: missing `=` in config entry\n",
              path, (unsigned long)lineno);
          break;
        }
        start = line.find_first_not_of(" \t\n\r", eq+1);
        line = move(line.substr(start));
        if (!fn(line))
          return false;
        break;
      }
    }
  }
  return true;
}

bool Config::ReadConfig() {
  string home(getenv("HOME"));
  string etcdir(PKGDEPDB_ETC);

  vec<string> config_paths({
    home   + "/.config/pkgdepdb/config",
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

} // ::pkgdepdb
