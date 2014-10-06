#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

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

static function<bool(string&)> cfg_json(uint &ref, string& err) {
  return [&ref,&err](string &line) -> bool {
    if (auto errstr = Config::ParseJSONBit(line.c_str(), ref)) {
      err = errstr;
      return false;
    }
    return true;
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

const char *Config::ParseJSONBit(const char *bit, uint &opt_json) {
  if (!*bit)
    return nullptr;
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
    return nullptr;
  }

  if (!strcmp(bit, "off") ||
      !strcmp(bit, "n")   ||
      !strcmp(bit, "no")  ||
      !strcmp(bit, "none") )
  {
    if (mode == 0)
      opt_json = 0;
    return nullptr;
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
    return nullptr;
  }

  if (!strcmp(bit, "db"))
  {
    if (mode == '+')
      opt_json |= JSONBits::DB;
    else if (mode == '-')
      opt_json &= ~JSONBits::DB;
    else
      opt_json = JSONBits::DB;
    return nullptr;
  }

  return "unknown json bit";
}

bool Config::ReadConfig(std::istream &in, const char *path) {
  string line, errstr="";

  tuple<string, function<bool(string&)>>
  rules[] = {
    make_tuple("database",         cfg_path(database_)),
    make_tuple("verbosity",        cfg_numeric(verbosity_)),
    make_tuple("quiet",            cfg_bool(quiet_)),
    make_tuple("package_depends",  cfg_bool(package_depends_)),
    make_tuple("json",             cfg_json(json_,errstr)),
    make_tuple("jobs",             cfg_numeric(max_jobs_)),
    make_tuple("file_lists",       cfg_bool(package_filelist_)),
  };

  size_t lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    size_t start = line.find_first_not_of(" \t\n\r");
    // no content
    if (start == string::npos)
      continue;
    // comment lines
    if (line[start] == '#' || line[start] == '/' || line[start] == ';')
      continue;

    errstr = "";
    for (auto &r : rules) {
      string &name(std::get<0>(r));
      auto    fn = std::get<1>(r);
      if (line.compare(start, name.length(), name) == 0) {
        size_t eq = line.find_first_not_of(" \t\n\r", start+name.length());
        if (eq == string::npos) {
          Log(Warn, "%s:%lu: invalid config entry\n",
              path, (unsigned long)lineno);
          break;
        }
        if (line[eq] != '=') {
          Log(Warn, "%s:%lu: missing `=` in config entry\n",
              path, (unsigned long)lineno);
          break;
        }
        start = line.find_first_not_of(" \t\n\r", eq+1);
        line = move(line.substr(start));
        if (!fn(line)) {
          if (!errstr.empty())
            Log(Error, "%s:%lu: %s\n", path, (unsigned long)lineno,
                errstr.c_str());
          return false;
        }
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

enum {
    RESET = 0,
    BOLD  = 1,
    BLACK = 30,
    RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, GRAY,
    WHITE = GRAY
};

void Config::Log(uint level, const char *msg, ...) const {
  if (level < log_level_)
    return;

  FILE *out = (level <= Message) ? stdout : stderr;

  if (level == Message) {
    if (isatty(fileno(out))) {
      fprintf(out, "\033[%d;%dm***\033[0;0m ", BOLD, GREEN);
    } else
      fprintf(out, "*** ");
  }
  va_list ap;
  va_start(ap, msg);
  vfprintf(out, msg, ap);
  va_end(ap);
}

} // ::pkgdepdb
