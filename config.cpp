#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <iostream>
#include <fstream>

#include "main.h"

static void clearpath(std::string &path) {
  // mhh, could strip whitespace at the end and allow
  // backslash escaping
  // or we just say screw you to people who use trailing
  // whitespaces in their configs...


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

static bool cfg_database(std::string &line) {
  clearpath(line);
  opt_default_db = line;
  return true;
}

template<typename T>
static std::function<bool(std::string&)> cfg_numeric(T &ref) {
  return [&ref](std::string &line) -> bool {
    ref = static_cast<T>(strtoul(line.c_str(), nullptr, 0));
    return true;
  };
}

bool CfgStrToBool(const std::string& line) {
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

static bool line_to_bool(std::string& line) {
  size_t n = line.find_first_of(" \t\r\n");
  if (n != std::string::npos)
    line = std::move(line.substr(0, n));
  return CfgStrToBool(line);
}

static std::function<bool(std::string&)> cfg_bool(bool &ref) {
  return [&ref](std::string &line) -> bool {
    ref = line_to_bool(line);
    return true;
  };
}

bool CfgParseJSONBit(const char *bit) {
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
      opt_json = static_cast<decltype(opt_json)>(-1);
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

static bool cfg_json(std::string &line) {
  (void)CfgParseJSONBit(line.c_str());
  return true;
}

static bool ReadConfig(std::istream &in, const char *path) {
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
      std::make_tuple("database",         cfg_database),
      std::make_tuple("verbosity",        cfg_numeric(opt_verbosity)),
      std::make_tuple("quiet",            cfg_bool(opt_quiet)),
      std::make_tuple("package_depends",  cfg_bool(opt_package_depends)),
      std::make_tuple("josn",             cfg_json),
      std::make_tuple("json",             cfg_json),
      std::make_tuple("jobs",             cfg_numeric(opt_max_jobs)),
      std::make_tuple("file_lists",       cfg_bool(opt_package_filelist)),
    };

    for (auto &r : rules) {
      std::string &name(std::get<0>(r));
      auto         fn = std::get<1>(r);
      if (line.compare(start, name.length(), name) == 0) {
        size_t eq = line.find_first_not_of(" \t\n\r", start+name.length());
        if (eq == std::string::npos) {
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
        line = std::move(line.substr(start));
        if (!fn(line))
          return false;
        break;
      }
    }
  }
  return true;
}

bool ReadConfig() {
  std::string home(getenv("HOME"));
  std::string etcdir(PKGDEPDB_ETC);

  std::vector<std::string> config_paths({
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
