#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"
#include "pkgdepdb.h"
#include "elf.h"
#include "package.h"
#include "db.h"
#include "filter.h"

using namespace pkgdepdb;

static const char *arg0 = 0;

static struct option long_opts[] = {
  { "help",    no_argument,       0, 'h' },
  { "version", no_argument,       0, -'v' },
  { "quiet",   no_argument,       0, 'q' },
  { "db",      required_argument, 0, 'd' },
  { "install", no_argument,       0, 'i' },
  { "dry",     no_argument,       0, -'d' },
  { "remove",  no_argument,       0, 'r' },
  { "info",    no_argument,       0, 'I' },
  { "list",    no_argument,       0, 'L' },
  { "missing", no_argument,       0, 'M' },
  { "found",   no_argument,       0, 'F' },
  { "pkgs",    no_argument,       0, 'P' },
  { "verbose", no_argument,       0, 'v' },
  { "rename",  required_argument, 0, 'n' },

  { "wipe",    no_argument,       0, -'W' },

  { "broken",  no_argument,       0, 'b' },
  { "not-empty",  no_argument,    0, -'E' },

  { "integrity",  no_argument,    0, -'G' },

  { "ld-append",  required_argument, 0, -'A' },
  { "ld-prepend", required_argument, 0, -'P' },
  { "ld-delete",  required_argument, 0, -'D' },
  { "ld-insert",  required_argument, 0, -'I' },
  { "ld-clear",   no_argument,       0, -'C' },

  { "rule",       required_argument, 0, 'R' },

  { "relink",     no_argument,       0, -'R' },

  { "json",       required_argument, 0, 'J' },

  { "fixpaths",   no_argument,       0, -'F' },

  { "depends",    required_argument, 0, -1024-'D' },

  { "filter",     required_argument, 0, 'f' },

  { "files",      optional_argument, 0, -1024-'f' },
  { "no-files",   no_argument,       0, -1025-'f' },
  { "ls",         no_argument,       0, -1026-'f' },
  { "rm-files",   no_argument,       0, -1027-'f' },

  { "touch",      no_argument,       0, -1024-'T' },

  { 0, 0, 0, 0 }
};

static void help [[noreturn]] (int x) {
  FILE *out = x ? stderr : stdout;
  fprintf(out,
    "usage: %s [options] packages...\n", arg0);
  fprintf(out,
    "options:\n"
    "  -h, --help         show this message\n"
    "  --version          show version info\n"
    "  -v, --verbose      print more information\n"
    "  -q, --quiet        suppress progress messages\n"
    "  --depends=<YES|NO> enable or disable package dependencies\n"
    "  --files=<YES|NO>   whether to store all non-object files of packages\n"
    "  -J, --json=PART    activate json mode for parts of the program\n"
#ifdef PKGDEPDB_ENABLE_THREADS
    "  -j N               limit to at most N threads\n"
#endif
               );
  fprintf(out,
    "json options:\n"
    "  off, n, none       no json output\n"
    "  on, q, query       use json on query outputs\n"
    "  db                 use json to store the db (NOT RECOMMENDED)\n"
    "  all, a             both\n"
    "  (optional + or - prefix to add or remove bits)\n"
    );
  fprintf(out,
    "db management options:\n"
    "  -d, --db=FILE      set the database file to commit to\n"
    "  -i, --install      install packages to a dependency db\n"
    "  -r, --remove       remove packages from the database\n"
    "  --dry              do not commit the changes to the db\n"
    "  --fixpaths         fix up path entries as older versions didn't\n"
    "                     handle ../ in paths (includes --relink)\n"
    "  -R, --rule=CMD     modify rules\n"
    "  --wipe             remove all packages, keep rules/settings\n"
    "  --touch            write out the db even without modifications\n"
    );
  fprintf(out,
    "db query options:\n"
    "  -I, --info         show general information about the db\n"
    "  -L, --list         list object files\n"
    "  -M, --missing      show the 'missing' table\n"
    "  -F, --found        show the 'found' table\n"
    "  -P, --pkgs         show the installed packages (and -v their files)\n"
    "  -n, --rename=NAME  rename the database\n"
    "  --integrity        perform a dependency integrity check\n"
    "  -f, --filter=FILT  filter the queried packages\n"
    "  --ls               list all package files\n"
    );
  fprintf(out,
    "db query filters:\n"
    "  -b, --broken       only packages with broken libs (use with -P)\n"
    "      --not-empty    filter out packages which are empty after filters\n"
    "                     have been applied\n"
    );
  fprintf(out,
    "db library path options: (run --relink after these changes)\n"
    "  --ld-prepend=DIR   add or move a directory to the\n"
    "                     top of the trusted library path\n"
    "  --ld-append=DIR    add or move a directory to the\n"
    "                     bottom of the trusted library path\n"
    "  --ld-delete=DIR    remove a library path\n"
    "  --ld-insert=N:DIR  add or move a directro to position N\n"
    "                     of the trusted library path\n"
    "  --ld-clear         remove all library paths\n"
    "  --relink           relink all objects\n"
    );
  fprintf(out,
    "rules for --rule:\n"
    "  strict:BOOL        set strict mode (default=off)\n"
    "  ignore:FILENAME    add a file-ignore rule\n"
    "  unignore:FILENAME  remove a file-ignore rule\n"
    "  unignore-id:ID     remove a file-ignore rule by its id\n"
    "  assume-found:NAME  add a file-ignore rule\n"
    "  unassume-found:LIBNAME\n"
    "                     remove a file-ignore rule\n"
    "  unassume-found-id:ID\n"
    "                     remove a file-ignore rule by its id\n"
    "  pkg-ld-clear:PKG   clear a pacakge's library path\n"
    "  pkg-ld-append:PKG:PATH\n"
    "  pkg-ld-prepend:PKG:PATH\n"
    "  pkg-ld-insert:PKG:ID:PATH\n"
    "                     add a path to the package's library path\n"
    "  pkg-ld-delete:PKG:PATH\n"
    "  pkg-ld-delete-id:PKG:ID\n"
    "                     delete a package's library path\n"
    "  base-add:PKG       add PKG to the base package list\n"
    "  base-remove:PKG    remove PKG form the base package list\n"
    "  base-remove-id:ID  remove form the base package list by id\n"
    );
  exit(x);
}

static void version [[noreturn]] (int x) {
  printf("pkgdepdb " PKGDEPDB_VERSION_STRING "\n");
  exit(x);
}

// don't ask
class ArgArg {
 public:
  bool        on_;
  bool       *mode_;
  vec<string> arg_;

  ArgArg(bool *om) {
    mode_ = om;
    on_ = false;
  }

  operator bool() const { return on_; }
  inline bool operator!() const { return !on_; }
  inline void operator=(bool on) {
    on_ = on;
    *mode_ = false;
  }
  inline void operator=(const char *arg) {
    on_ = true;
    *mode_ = false;
    arg_.push_back(arg);
  }
};

static bool parse_rule(DB *db, const string& rule);
static bool parse_filter(const string &filter,
                         FilterList&,
                         ObjFilterList&,
                         StrFilterList&);

int main(int argc, char **argv) {
  arg0 = argv[0];

  if (argc < 2)
    help(1);

  string dbfile,
         newname;
  bool   do_install    = false;
  bool   do_delete     = false;
  bool   do_wipe       = false;
  bool   do_wipefiles  = false;
  bool   has_db        = false;
  bool   modified      = false;
  bool   show_info     = false;
  bool   show_list     = false;
  bool   show_missing  = false;
  bool   show_found    = false;
  bool   show_packages = false;
  bool   show_filelist = false;
  bool   do_rename     = false;
  bool   do_relink     = false;
  bool   do_fixpaths   = false;
  bool   dryrun        = false;
  bool   filter_broken = false;
  bool   filter_nempty = false;
  bool   do_integrity  = false;

  bool   oldmode       = true;

  // library path options
  ArgArg ld_append (&oldmode),
         ld_prepend(&oldmode),
         ld_delete (&oldmode),
         ld_clear  (&oldmode),
         rulemod   (&oldmode);
  vec<std::tuple<string,size_t>> ld_insert;

  FilterList pkg_filters;
  ObjFilterList obj_filters;
  StrFilterList str_filters;

  Config config;
  config.log_level_ = LogLevel::Message;

  if (!config.ReadConfig())
    return 1;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunreachable-code"
  for (;;) {
    int opt_index = 0;
    int c = getopt_long(argc, argv, "hvqird:ILMFPbn:R:J:j:f:",
                        long_opts, &opt_index);
    if (c == -1)
      break;
    switch (c) {
      case 'h':
        help(0);
        break;
      case -'v':
        version(0);
        break;
      case 'q':
        config.quiet_ = true;
        break;
      case 'd':
        oldmode = false;
        has_db = true;
        dbfile = optarg;
        break;

      case 'n':
        oldmode = false;
        do_rename = true;
        newname = optarg;
        break;

      case -'d': dryrun = true; break;

      case 'v': ++config.verbosity_; break;

      case 'i':  oldmode = false; do_install    = true; break;
      case 'r':  oldmode = false; do_delete     = true; break;
      case -'W': oldmode = false; do_wipe       = true; break;
      case 'I':  oldmode = false; show_info     = true; break;
      case 'L':  oldmode = false; show_list     = true; break;
      case 'M':  oldmode = false; show_missing  = true; break;
      case 'F':  oldmode = false; show_found    = true; break;
      case 'P':  oldmode = false; show_packages = true; break;
      case 'b':  oldmode = false; filter_broken = true; break;

      case -'E': oldmode = false; filter_nempty = true; break;

      case -'R': oldmode = false; do_relink   = true; break;
      case -'F': oldmode = false; do_fixpaths = true; break;

      case -'G': oldmode = false; do_integrity = true; break;

      case -1024-'T': oldmode = false; modified = true; break;

      case -1024-'D':
        config.package_depends_ = Config::str2bool(optarg);
        break;

      case -1024-'f':
        if (optarg)
          config.package_filelist_ = Config::str2bool(optarg);
        else
          config.package_filelist_ = true;
        break;
      case -1025-'f':
        config.package_filelist_ = false;
        break;
      case -1026-'f':
        oldmode = false;
        show_filelist = true;
        break;
      case -1027-'f':
        oldmode = false;
        do_wipefiles = true;
        break;

      case  'R': rulemod    = optarg; break;
      case -'A': ld_append  = optarg; break;
      case -'P': ld_prepend = optarg; break;
      case -'D': ld_delete  = optarg; break;
      case -'C': ld_clear   = true;   break;
      case -'I':
      {
        oldmode = false;
        string str(optarg);
        if (!isdigit(str[0])) {
          fprintf(stderr,
                  "--ld-insert format wrong: has to start with a number\n");
          help(1);
          return 1;
        }
        size_t colon = str.find_first_of(':');
        if (string::npos == colon) {
          fprintf(stderr, "--ld-insert format wrong, no colon found\n");
          help(1);
          return 1;
        }
        ld_insert.push_back(std::make_tuple(move(str.substr(colon+1)),
                                            strtoul(str.c_str(), nullptr, 0)));
        break;
      }

      case 'J':
        if (!Config::ParseJSONBit(optarg, config.json_))
          help(1);
        break;

      case 'j':
        config.max_jobs_ = (unsigned int)strtoul(optarg, nullptr, 0);
        // some sanity:
        if (config.max_jobs_ > 128)
          config.max_jobs_ = 128;
        break;

      case 'f':
        if (!parse_filter(optarg, pkg_filters, obj_filters, str_filters)) {
          fprintf(stderr, "invalid --filter: `%s'\n", optarg);
          return 1;
        }
        break;

      case ':':
      case '?':
        help(1);
        break;
    }
  }
#pragma clang diagnostic pop

  if (config.quiet_)
    config.log_level_ = LogLevel::Print;

  if (do_fixpaths)
    do_relink = true;

  if (do_install && (do_delete || do_wipe)) {
    fprintf(stderr, "--install and --remove/--wipe are mutually exclusive\n");
    help(1);
  }

  if (do_delete && optind >= argc) {
    fprintf(stderr, "--remove requires a list of package names\n");
    help(1);
  }

  if (do_install && optind >= argc) {
    fprintf(stderr, "--install requires a list of package archive files\n");
    help(1);
  }

  vec<Package*> packages;

  if (oldmode) {
    // non-database mode!
    if (optind >= argc)
      help(1);
    while (optind < argc) {
      Package *package = Package::Open(argv[optind++], config);
      package->ShowNeeded();
      delete package;
    }
    return 0;
  }

  if (!has_db) {
    if (config.database_.length()) {
      has_db = true;
      dbfile = move(config.database_);
    }
  }

  if (!has_db) {
    fprintf(stderr, "no database selected\n");
    return 0;
  }

  if (!do_delete && optind < argc) {
    if (do_install)
      config.Log(Message, "loading packages...\n");

    while (optind < argc) {
      if (do_install)
        config.Log(Print, "  %s\n", argv[optind]);
      Package *package = Package::Open(argv[optind], config);
      if (!package)
        config.Log(Error, "error reading package %s\n", argv[optind]);
      else {
        if (do_install)
          packages.push_back(package);
        else {
          package->ShowNeeded();
          delete package;
        }
      }
      ++optind;
    }

    if (do_install)
      config.Log(Message, "packages loaded...\n");
  }

  uniq<DB> db(new DB(config));
  if (has_db) {
    if (!db->Read(dbfile)) {
      config.Log(Error, "failed to read database\n");
      return 1;
    }
  }

  if (do_rename) {
    modified = true;
    db->name_ = newname;
  }

  if (rulemod) {
    for (auto &rule : rulemod.arg_)
      modified = parse_rule(db.get(), rule) || modified;
  }

  if (ld_append) {
    for (auto &dir : ld_append.arg_)
      modified = db->LD_Append(dir)  || modified;
  }
  if (ld_prepend) {
    for (auto &dir : ld_prepend.arg_)
      modified = db->LD_Prepend(dir) || modified;
  }
  if (ld_delete) {
    for (auto &dir : ld_delete.arg_)
      modified = db->LD_Delete(dir)  || modified;
  }
  for (auto &ins : ld_insert) {
    modified = db->LD_Insert(std::get<0>(ins), std::get<1>(ins))
             || modified;
  }
  if (ld_clear)
    modified = db->LD_Clear() || modified;

  if (do_wipe)
    modified = db->WipePackages() || modified;

  if (do_fixpaths) {
    modified = true;
    config.Log(Message, "fixing up path entries\n");
    db->FixPaths();
  }

  if (do_install && packages.size()) {
    config.Log(Message, "installing packages\n");
    for (auto pkg : packages) {
      modified = true;
      if (!db->InstallPackage(move(pkg))) {
        printf("failed to commit package %s to database\n",
               pkg->name_.c_str());
        break;
      }
    }
  }

  if (do_delete) {
    while (optind < argc) {
      config.Log(Message, "uninstalling: %s\n", argv[optind]);
      modified = true;
      if (!db->DeletePackage(argv[optind])) {
        config.Log(Error, "error uninstalling package: %s\n", argv[optind]);
        return 1;
      }
      ++optind;
    }
  }

  if (do_relink) {
    modified = true;
    config.Log(Message, "relinking everything\n");
    db->RelinkAll();
  }

  if (do_wipefiles)
    modified = db->WipeFilelists();

  if (show_info)
    db->ShowInfo();

  if (show_packages)
    db->ShowPackages(filter_broken, filter_nempty, pkg_filters, obj_filters);

  if (show_list)
    db->ShowObjects(pkg_filters, obj_filters);

  if (show_missing)
    db->ShowMissing();

  if (show_found)
    db->ShowFound();

  if (show_filelist)
    db->ShowFilelist(pkg_filters, str_filters);

  if (do_integrity)
    db->CheckIntegrity(pkg_filters, obj_filters);

  if (!dryrun && modified && has_db) {
    if (config.json_ & JSONBits::DB)
      db_store_json(db.get(), dbfile);
    else if (!db->Store(dbfile))
      config.Log(Error, "failed to write to the database\n");
  }

  return 0;
}

static bool try_rule(const string                 &rule,
                     const string                 &what,
                     const char                   *usage,
                     bool                         *ret,
                     function<bool(const string&)> fn)
{
  if (rule.compare(0, what.length(), what) == 0) {
    if (rule.length() < what.length()+1) {
      fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
      fprintf(stderr, "format: %s%s\n", what.c_str(), usage);
      *ret = false;
      return true;
    }
    *ret = fn(rule.substr(what.length()));
    return true;
  }
  return false;
}

static bool parse_rule(DB *db, const string& rule) {
  bool ret = false;

  if (try_rule(rule, "ignore:", "FILENAME", &ret,
    [db](const string &cmd) {
      return db->IgnoreFile_Add(cmd);
    })
    || try_rule(rule, "strict:", "BOOL", &ret,
    [db](const string &cmd) {
      bool old = db->strict_linking_;
      db->strict_linking_ = Config::str2bool(cmd);
      return old == db->strict_linking_;
    })
    || try_rule(rule, "unignore:", "FILENAME", &ret,
    [db](const string &cmd) {
      return db->IgnoreFile_Delete(cmd);
    })
    || try_rule(rule, "unignore-id:", "ID", &ret,
    [db](const string &cmd) {
      return db->IgnoreFile_Delete(strtoul(cmd.c_str(), nullptr, 0));
    })
    || try_rule(rule, "assume-found:", "LIBNAME", &ret,
    [db](const string &cmd) {
      return db->AssumeFound_Add(cmd);
    })
    || try_rule(rule, "unassume-found:", "LIBNAME", &ret,
    [db](const string &cmd) {
      return db->AssumeFound_Delete(cmd);
    })
    || try_rule(rule, "unassume-found:", "ID", &ret,
    [db](const string &cmd) {
      return db->AssumeFound_Delete(strtoul(cmd.c_str(), nullptr, 0));
    })
    || try_rule(rule, "pkg-ld-clear:", "PKG", &ret,
    [db,&rule](const string &cmd) {
      return db->PKG_LD_Clear(cmd);
    })
    || try_rule(rule, "pkg-ld-append:", "PKG:PATH", &ret,
    [db,&rule](const string &cmd) {
      size_t s = cmd.find_first_of(':');
      if (s == string::npos) {
        fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
        fprintf(stderr, "format: pkg-ld-append:PKG:PATH\n");
        return false;
      }
      string pkg(cmd.substr(0, s));
      StringList &lst(db->package_library_path_[pkg]);
      return db->PKG_LD_Insert(pkg, cmd.substr(s+1), lst.size());
    })
    || try_rule(rule, "pkg-ld-prepend:", "PKG:PATH", &ret,
    [db,&rule](const string &cmd) {
      size_t s = cmd.find_first_of(':');
      if (s == string::npos) {
        fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
        fprintf(stderr, "format: pkg-ld-prepend:PKG:PATH\n");
        return false;
      }
      return db->PKG_LD_Insert(cmd.substr(0, s), cmd.substr(s+1), 0);
    })
    || try_rule(rule, "pkg-ld-insert:", "PKG:ID:PATH", &ret,
    [db,&rule](const string &cmd) {
      size_t s1, s2 = 0;
      s1 = cmd.find_first_of(':');
      if (s1 != string::npos)
        s2 = cmd.find_first_of(':', s1+1);
      if (s1 == string::npos ||
          s2 == string::npos)
      {
        fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
        fprintf(stderr, "format: pkg-ld-insert:PKG:ID:PATH\n");
        return false;
      }
      return db->PKG_LD_Insert(cmd.substr(0, s1),
                               cmd.substr(s2+1),
                               strtoul(cmd.substr(s1, s2-s1).c_str(),
                                       nullptr, 0));
    })
    || try_rule(rule, "pkg-ld-delete:", "PKG:PATH", &ret,
    [db,&rule](const string &cmd) {
      size_t s = cmd.find_first_of(':');
      if (s == string::npos) {
        fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
        fprintf(stderr, "format: pkg-ld-delete:PKG:PATH\n");
        return false;
      }
      return db->PKG_LD_Delete(cmd.substr(0, s), cmd.substr(s+1));
    })
    || try_rule(rule, "pkg-ld-delete-id:", "PKG:ID", &ret,
    [db,&rule](const string &cmd) {
      size_t s = cmd.find_first_of(':');
      if (s == string::npos) {
        fprintf(stderr, "malformed rule: `%s`\n", rule.c_str());
        fprintf(stderr, "format: pkg-ld-delete-id:PKG:ID\n");
        return false;
      }
      return db->PKG_LD_Delete(cmd.substr(0, s),
                               strtoul(cmd.substr(s+1).c_str(), nullptr, 0));
    })
    || try_rule(rule, "base-add:", "PKG", &ret,
    [db,&rule](const string &cmd) {
      return db->BasePackages_Add(cmd);
    })
    || try_rule(rule, "base-remove:", "PKG", &ret,
    [db,&rule](const string &cmd) {
      return db->BasePackages_Delete(cmd);
    })
    || try_rule(rule, "base-remove-id:", "ID", &ret,
    [db,&rule](const string &cmd) {
      return db->BasePackages_Delete(strtoul(cmd.c_str(), nullptr, 0));
    })
  ) {
    return ret;
  }
  fprintf(stderr, "no such rule command: `%s'\n", rule.c_str());
  return false;
}

static bool parse_filter(const string  &filter,
                         FilterList    &pkg_filters,
                         ObjFilterList &obj_filters,
                         StrFilterList &str_filters)
{
  // -fname=foo exact
  // -fname:foo glob
  // -fname/foo/ regex
  // -fname/foo/i iregex
  // the manpage calls REG_BASIC "obsolete" so we default to extended

  bool neg = false;
  size_t at = 0;
  while (filter.length() > at && filter[at] == '!') {
    neg = !neg;
    ++at;
  }

  if (filter.compare(at, string::npos, "broken") == 0) {
    auto pf = filter::PackageFilter::broken(neg);
    if (!pf)
      return false;
    pkg_filters.push_back(move(pf));
    return true;
  }

#ifdef WITH_REGEX
  string regex;
  bool icase = false;
  auto parse_regex = [&]() -> bool {
    if (static_cast<unsigned char>(filter[at] - 'a') > ('z'-'a') &&
        static_cast<unsigned char>(filter[at] - 'A') > ('Z'-'A') &&
        static_cast<unsigned char>(filter[at] - '0') > ('9'-'0'))
    {
      // parse the regex enclosed using the character from filter[4]
      char unquote = filter[at];
      if      (unquote == '(') unquote = ')';
      else if (unquote == '{') unquote = '}';
      else if (unquote == '[') unquote = ']';
      else if (unquote == '<') unquote = '>';
      if (filter.length() < at+2) {
        fprintf(stderr, "empty filter content: %s\n", filter.c_str());
        return false;
      }
      regex = filter.substr(at+1);
      icase = false;
      if (regex[regex.length()-1] == 'i') {
        if (regex[regex.length()-2] == unquote) {
          icase = true;
          regex.erase(regex.length()-2);
        }
      }
      else if (regex[regex.length()-1] == unquote)
        regex.pop_back();
      return true;
    }
    return false;
  };
#endif

  auto parsematch = [&]() -> rptr<filter::Match> {
    if (filter[at] == '=')
      return filter::Match::CreateExact(move(filter.substr(at+1)));
    else if (filter[at] == ':')
      return filter::Match::CreateGlob(move(filter.substr(at+1)));
#ifdef WITH_REGEX
    else if (parse_regex())
      return filter::Match::CreateRegex(move(regex), icase);
#endif
    return nullptr;
  };

#define ADDFILTER2(TYPE, NAME, FUNC, DEST) do {                 \
  if (filter.compare(at, sizeof(#NAME)-1, #NAME) == 0) {        \
    at += sizeof(#NAME)-1;                                      \
    auto match = parsematch();                                  \
    if (!match)                                                 \
      return false;                                             \
    DEST.push_back(move(filter::TYPE::FUNC(move(match), neg))); \
    return true;                                                \
  } } while(0)

#define ADDFILTER(TYPE, NAME, DEST) ADDFILTER2(TYPE, NAME, NAME, DEST)

#define MAKE_PKGFILTER(NAME) ADDFILTER(PackageFilter, NAME, pkg_filters)
  MAKE_PKGFILTER(name);
  MAKE_PKGFILTER(group);
  MAKE_PKGFILTER(depends);
  MAKE_PKGFILTER(optdepends);
  MAKE_PKGFILTER(alldepends);
  MAKE_PKGFILTER(provides);
  MAKE_PKGFILTER(conflicts);
  MAKE_PKGFILTER(replaces);
  MAKE_PKGFILTER(pkglibdepends);
  MAKE_PKGFILTER(pkglibrpath);
  MAKE_PKGFILTER(pkglibrunpath);
  MAKE_PKGFILTER(pkglibinterp);
  MAKE_PKGFILTER(contains);
#undef MAKE_PKGFILTER

#define MAKE_OBJFILTER(NAME) ADDFILTER2(ObjectFilter, lib##NAME, NAME, obj_filters)
  MAKE_OBJFILTER(name);
  MAKE_OBJFILTER(depends);
  MAKE_OBJFILTER(path);
  MAKE_OBJFILTER(rpath);
  MAKE_OBJFILTER(runpath);
  MAKE_OBJFILTER(interp);
#undef MAKE_OBJFILTER

  ADDFILTER2(StringFilter, file, filter, str_filters);

  return false;
}
