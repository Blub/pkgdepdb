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

static int LogLevel = Message;
static const char *arg0 = 0;

std::string   opt_default_db = "";
unsigned int  opt_verbosity = 0;
unsigned int  opt_json      = 0;
unsigned int  opt_max_jobs  = 0;
bool          opt_quiet     = false;
bool          opt_package_depends = true;
bool          opt_package_filelist = false;

enum {
    RESET = 0,
    BOLD  = 1,
    BLACK = 30,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    GRAY,
    WHITE = GRAY
};

void
log(int level, const char *msg, ...)
{
	if (level < LogLevel)
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

	{ 0, 0, 0, 0 }
};

static void
help(int x)
{
	FILE *out = x ? stderr : stdout;
	fprintf(out, "usage: %s [options] packages...\n", arg0);
	fprintf(out, "options:\n"
	             "  -h, --help         show this message\n"
	             "  --version          show version info\n"
	             "  -v, --verbose      print more information\n"
	             "  -q, --quiet        suppress progress messages\n"
	             "  --depends=<YES|NO> enable or disable package dependencies\n"
	             "  -J, --json=PART    activate json mode for parts of the program\n"
#ifdef ENABLE_THREADS
	             "  -j N               limit to at most N threads\n"
#endif
	             );
	fprintf(out, "json options:\n"
	             "  off, n, none       no json output\n"
	             "  on, q, query       use json on query outputs\n"
	             "  db                 use json to store the db (NOT RECOMMENDED)\n"
	             "  all, a             both\n"
	             "  (optional + or - prefix to add or remove bits)\n"
	             );
	fprintf(out, "db management options:\n"
	             "  -d, --db=FILE      set the database file to commit to\n"
	             "  -i, --install      install packages to a dependency db\n"
	             "  -r, --remove       remove packages from the database\n"
	             "  --dry              do not commit the changes to the db\n"
	             "  --fixpaths         fix up path entries as older versions didn't\n"
	             "                     handle ../ in paths (includes --relink)\n"
	             "  -R, --rule=CMD     modify rules\n"
	             "  --wipe             remove all packages, keep rules/settings\n"
	             );
	fprintf(out, "db query options:\n"
	             "  -I, --info         show general information about the db\n"
	             "  -L, --list         list object files\n"
	             "  -M, --missing      show the 'missing' table\n"
	             "  -F, --found        show the 'found' table\n"
	             "  -P, --pkgs         show the installed packages (and -v their files)\n"
	             "  -n, --rename=NAME  rename the database\n"
	             "  --integrity        perform a dependency integrity check\n"
	             "  -f, --filter=FILT  filter the queried packages\n"
	             );
	fprintf(out, "db query filters:\n"
	             "  -b, --broken       only packages with broken libs (use with -P)\n"
	             );
	fprintf(out, "db library path options: (run --relink after these changes)\n"
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
	fprintf(out, "rules for --rule:\n"
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

static void
version(int x)
{
	printf("pkgdepdb " FULL_VERSION_STRING "\n");
	exit(x);
}

// don't ask
class ArgArg {
public:
	bool        on;
	bool       *mode;
	std::vector<std::string> arg;

	ArgArg(bool *om) {
		mode = om;
		on = false;
	}

	operator bool() const { return on; }
	inline bool operator!() const { return !on; }
	inline void operator=(bool on) {
		this->on = on;
		*mode = false;
	}
	inline void operator=(const char *arg) {
		on = true;
		*mode = false;
		this->arg.push_back(arg);
	}
};

static bool parse_rule(DB *db, const std::string& rule);
static bool parse_filter(const std::string &filter, FilterList&, ObjFilterList&);

bool db_store_json(DB *db, const std::string& filename);

int
main(int argc, char **argv)
{
	arg0 = argv[0];

	if (argc < 2)
		help(1);

	std::string  dbfile, newname;
	bool         do_install    = false;
	bool         do_delete     = false;
	bool         do_wipe       = false;
	bool         has_db        = false;
	bool         modified      = false;
	bool         show_info     = false;
	bool         show_list     = false;
	bool         show_missing  = false;
	bool         show_found    = false;
	bool         show_packages = false;
	bool         do_rename     = false;
	bool         do_relink     = false;
	bool         do_fixpaths   = false;
	bool         dryrun        = false;
	bool         filter_broken = false;
	bool         do_integrity  = false;
	bool oldmode = true;

	// library path options
	ArgArg ld_append (&oldmode),
	       ld_prepend(&oldmode),
	       ld_delete (&oldmode),
	       ld_clear  (&oldmode),
	       rulemod   (&oldmode);
	std::vector<std::tuple<std::string,size_t>> ld_insert;

	FilterList pkg_filters;
	ObjFilterList obj_filters;

	LogLevel = Message;
	if (!ReadConfig())
		return 1;
	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hvqird:ILMFPbn:R:J:j:f:", long_opts, &opt_index);
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
				opt_quiet = true;
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

			case 'v': ++opt_verbosity; break;

			case 'i':  oldmode = false; do_install    = true; break;
			case 'r':  oldmode = false; do_delete     = true; break;
			case -'W': oldmode = false; do_wipe       = true; break;
			case 'I':  oldmode = false; show_info     = true; break;
			case 'L':  oldmode = false; show_list     = true; break;
			case 'M':  oldmode = false; show_missing  = true; break;
			case 'F':  oldmode = false; show_found    = true; break;
			case 'P':  oldmode = false; show_packages = true; break;
			case 'b':  oldmode = false; filter_broken = true; break;

			case -'R': oldmode = false; do_relink   = true; break;
			case -'F': oldmode = false; do_fixpaths = true; break;

			case -'G': oldmode = false; do_integrity = true; break;

			case -1024-'D':
				opt_package_depends = CfgStrToBool(optarg);
				break;

			case  'R': rulemod    = optarg; break;
			case -'A': ld_append  = optarg; break;
			case -'P': ld_prepend = optarg; break;
			case -'D': ld_delete  = optarg; break;
			case -'C': ld_clear   = true;   break;
			case -'I':
			{
				oldmode = false;
				std::string str(optarg);
				if (!isdigit(str[0])) {
					log(Error, "--ld-insert format wrong: has to start with a number\n");
					help(1);
					return 1;
				}
				size_t colon = str.find_first_of(':');
				if (std::string::npos == colon) {
					log(Error, "--ld-insert format wrong, no colon found\n");
					help(1);
					return 1;
				}
				ld_insert.push_back(std::make_tuple(std::move(str.substr(colon+1)),
				                                    strtoul(str.c_str(), nullptr, 0)));
				break;
			}

			case 'J':
				if (!CfgParseJSONBit(optarg))
					help(1);
				break;

			case 'j':
				opt_max_jobs = strtoul(optarg, nullptr, 0);
				// some sanity:
				if (opt_max_jobs > 32)
					opt_max_jobs = 32;
				break;

			case 'f':
				if (!parse_filter(optarg, pkg_filters, obj_filters)) {
					log(Error, "invalid --filter: `%s'\n", optarg);
					return 1;
				}
				break;

			case ':':
			case '?':
				help(1);
				break;
		}
	}

	if (opt_quiet)
		LogLevel = Print;

	if (do_fixpaths)
		do_relink = true;

	if (do_install && (do_delete || do_wipe)) {
		log(Error, "--install and --remove/--wipe are mutually exclusive\n");
		help(1);
	}

	if (do_delete && optind >= argc) {
		log(Error, "--remove requires a list of package names\n");
		help(1);
	}

	if (do_install && optind >= argc) {
		log(Error, "--install requires a list of package archive files\n");
		help(1);
	}

	std::vector<Package*> packages;

	if (oldmode) {
		// non-database mode!
		if (optind >= argc)
			help(1);
		while (optind < argc) {
			Package *package = Package::open(argv[optind++]);
			package->show_needed();
			delete package;
		}
		return 0;
	}

	if (!has_db) {
		if (opt_default_db.length()) {
			has_db = true;
			dbfile = std::move(opt_default_db);
		}
	}

	if (!has_db) {
		log(Message, "no database selected\n");
		return 0;
	}

	if (!do_delete && optind < argc) {
		if (do_install)
			log(Message, "loading packages...\n");

		while (optind < argc) {
			if (do_install)
				log(Print, "  %s\n", argv[optind]);
			Package *package = Package::open(argv[optind]);
			if (!package)
				log(Error, "error reading package %s\n", argv[optind]);
			else {
				if (do_install)
					packages.push_back(package);
				else {
					package->show_needed();
					delete package;
				}
			}
			++optind;
		}

		if (do_install)
			log(Message, "packages loaded...\n");
	}

	std::unique_ptr<DB> db(new DB);
	if (has_db) {
		if (!db->read(dbfile)) {
			log(Error, "failed to read database\n");
			return 1;
		}
	}

	if (do_rename) {
		modified = true;
		db->name = newname;
	}

	if (rulemod) {
		for (auto &rule : rulemod.arg)
			modified = parse_rule(db.get(), rule) || modified;
	}

	if (ld_append) {
		for (auto &dir : ld_append.arg)
			modified = db->ld_append(dir)  || modified;
	}
	if (ld_prepend) {
		for (auto &dir : ld_prepend.arg)
			modified = db->ld_prepend(dir) || modified;
	}
	if (ld_delete) {
		for (auto &dir : ld_delete.arg)
			modified = db->ld_delete(dir)  || modified;
	}
	for (auto &ins : ld_insert) {
		modified = db->ld_insert(std::get<0>(ins), std::get<1>(ins))
		         || modified;
	}
	if (ld_clear)
		modified = db->ld_clear() || modified;

	if (do_wipe)
		modified = db->wipe_packages() || modified;

	if (do_fixpaths) {
		modified = true;
		log(Message, "fixing up path entries\n");
		db->fix_paths();
	}

	if (do_install && packages.size()) {
		log(Message, "installing packages\n");
		for (auto pkg : packages) {
			modified = true;
			if (!db->install_package(std::move(pkg))) {
				printf("failed to commit package %s to database\n", pkg->name.c_str());
				break;
			}
		}
	}

	if (do_delete) {
		while (optind < argc) {
			log(Message, "uninstalling: %s\n", argv[optind]);
			modified = true;
			if (!db->delete_package(argv[optind])) {
				log(Error, "error uninstalling package: %s\n", argv[optind]);
				return 1;
			}
			++optind;
		}
	}

	if (do_relink) {
		modified = true;
		log(Message, "relinking everything\n");
		db->relink_all();
	}

	if (show_info)
		db->show_info();

	if (show_packages)
		db->show_packages(filter_broken, pkg_filters, obj_filters);

	if (show_list)
		db->show_objects(pkg_filters, obj_filters);

	if (show_missing)
		db->show_missing();

	if (show_found)
		db->show_found();

	if (do_integrity)
		db->check_integrity(pkg_filters, obj_filters);

	if (!dryrun && modified && has_db) {
		if (opt_json & JSONBits::DB)
			db_store_json(db.get(), dbfile);
		else if (!db->store(dbfile))
			log(Error, "failed to write to the database\n");
	}

	return 0;
}

static bool
try_rule(const std::string& rule,
         const std::string& what,
         const char        *usage,
         bool              *ret,
         std::function<bool(const std::string&)> fn)
{
	if (rule.compare(0, what.length(), what) == 0) {
		if (rule.length() < what.length()+1) {
			log(Error, "malformed rule: `%s`\n", rule.c_str());
			log(Error, "format: %s%s\n", what.c_str(), usage);
			*ret = false;
			return true;
		}
		*ret = fn(rule.substr(what.length()));
		return true;
	}
	return false;
}

static bool
parse_rule(DB *db, const std::string& rule)
{
	bool ret = false;

	if (try_rule(rule, "ignore:", "FILENAME", &ret,
		[db](const std::string &cmd) {
			return db->ignore_file(cmd);
		})
		|| try_rule(rule, "strict:", "BOOL", &ret,
		[db](const std::string &cmd) {
			bool old = db->strict_linking;
			db->strict_linking = CfgStrToBool(cmd);
			return old == db->strict_linking;
		})
		|| try_rule(rule, "unignore:", "FILENAME", &ret,
		[db](const std::string &cmd) {
			return db->unignore_file(cmd);
		})
		|| try_rule(rule, "unignore-id:", "ID", &ret,
		[db](const std::string &cmd) {
			return db->unignore_file(strtoul(cmd.c_str(), nullptr, 0));
		})
		|| try_rule(rule, "assume-found:", "LIBNAME", &ret,
		[db](const std::string &cmd) {
			return db->assume_found(cmd);
		})
		|| try_rule(rule, "unassume-found:", "LIBNAME", &ret,
		[db](const std::string &cmd) {
			return db->unassume_found(cmd);
		})
		|| try_rule(rule, "unassume-found:", "ID", &ret,
		[db](const std::string &cmd) {
			return db->unassume_found(strtoul(cmd.c_str(), nullptr, 0));
		})
		|| try_rule(rule, "pkg-ld-clear:", "PKG", &ret,
		[db,&rule](const std::string &cmd) {
			return db->pkg_ld_clear(cmd);
		})
		|| try_rule(rule, "pkg-ld-append:", "PKG:PATH", &ret,
		[db,&rule](const std::string &cmd) {
			size_t s = cmd.find_first_of(':');
			if (s == std::string::npos) {
				log(Error, "malformed rule: `%s`\n", rule.c_str());
				log(Error, "format: pkg-ld-append:PKG:PATH\n");
				return false;
			}
			std::string pkg(cmd.substr(0, s));
			StringList &lst(db->package_library_path[pkg]);
			return db->pkg_ld_insert(pkg, cmd.substr(s+1), lst.size());
		})
		|| try_rule(rule, "pkg-ld-prepend:", "PKG:PATH", &ret,
		[db,&rule](const std::string &cmd) {
			size_t s = cmd.find_first_of(':');
			if (s == std::string::npos) {
				log(Error, "malformed rule: `%s`\n", rule.c_str());
				log(Error, "format: pkg-ld-prepend:PKG:PATH\n");
				return false;
			}
			return db->pkg_ld_insert(cmd.substr(0, s), cmd.substr(s+1), 0);
		})
		|| try_rule(rule, "pkg-ld-insert:", "PKG:ID:PATH", &ret,
		[db,&rule](const std::string &cmd) {
			size_t s1, s2;
			s1 = cmd.find_first_of(':');
			if (s1 != std::string::npos)
				s2 = cmd.find_first_of(':', s1+1);
			if (s1 == std::string::npos ||
			    s2 == std::string::npos)
			{
				log(Error, "malformed rule: `%s`\n", rule.c_str());
				log(Error, "format: pkg-ld-insert:PKG:ID:PATH\n");
				return false;
			}
			return db->pkg_ld_insert(cmd.substr(0, s1),
			                         cmd.substr(s2+1),
			                         strtoul(cmd.substr(s1, s2-s1).c_str(), nullptr, 0));
		})
		|| try_rule(rule, "pkg-ld-delete:", "PKG:PATH", &ret,
		[db,&rule](const std::string &cmd) {
			size_t s = cmd.find_first_of(':');
			if (s == std::string::npos) {
				log(Error, "malformed rule: `%s`\n", rule.c_str());
				log(Error, "format: pkg-ld-delete:PKG:PATH\n");
				return false;
			}
			return db->pkg_ld_delete(cmd.substr(0, s), cmd.substr(s+1));
		})
		|| try_rule(rule, "pkg-ld-delete-id:", "PKG:ID", &ret,
		[db,&rule](const std::string &cmd) {
			size_t s = cmd.find_first_of(':');
			if (s == std::string::npos) {
				log(Error, "malformed rule: `%s`\n", rule.c_str());
				log(Error, "format: pkg-ld-delete-id:PKG:ID\n");
				return false;
			}
			return db->pkg_ld_delete(cmd.substr(0, s), strtoul(cmd.substr(s+1).c_str(), nullptr, 0));
		})
		|| try_rule(rule, "base-add:", "PKG", &ret,
		[db,&rule](const std::string &cmd) {
			return db->add_base_package(cmd);
		})
		|| try_rule(rule, "base-remove:", "PKG", &ret,
		[db,&rule](const std::string &cmd) {
			return db->remove_base_package(cmd);
		})
		|| try_rule(rule, "base-remove-id:", "ID", &ret,
		[db,&rule](const std::string &cmd) {
			return db->remove_base_package(strtoul(cmd.c_str(), nullptr, 0));
		})
	) {
		return ret;
	}
	log(Error, "no such rule command: `%s'\n", rule.c_str());
	return false;
}

static bool
parse_filter(const std::string &filter, FilterList &pkg_filters, ObjFilterList &obj_filters)
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

#ifdef WITH_REGEX
	std::string regex;
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
				log(Error, "empty filter content: %s\n", filter.c_str());
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

	if (filter.compare(at, 4, "name") == 0) {
		at += 4;
		unique_ptr<filter::PackageFilter> pf(nullptr);
		if (filter[at] == '=') // exact
			pf = move(filter::PackageFilter::name(filter.substr(at+1), neg));
		else if (filter[at] == ':') // glob
			pf = move(filter::PackageFilter::nameglob(filter.substr(at+1), neg));
#ifdef WITH_REGEX
		else if (parse_regex())
			pf = move(filter::PackageFilter::nameregex(regex, true, icase, neg));
#endif
		else {
			log(Error, "unknown name filter: %s\n", filter.c_str());
			return false;
		}
		if (!pf) {
			log(Error, "failed to create filter: %s\n", filter.c_str());
			return false;
		}
		pkg_filters.push_back(move(pf));
		return true;
	}
#ifdef WITH_REGEX
# define MAKE_PKGFILTER_REGEXPART(NAME)                                       \
		else if (parse_regex())                                                   \
			pf = move(filter::PackageFilter::NAME##regex(regex, true, icase, neg));
#else
# define MAKE_PKGFILTER_REGEXPART(NAME)
#endif
# define MAKE_PKGFILTER(NAME) \
	else if (filter.compare(at, sizeof(#NAME)-1, #NAME) == 0) {                 \
		at += sizeof(#NAME)-1;                                                    \
		unique_ptr<filter::PackageFilter> pf(nullptr);                            \
		if (filter[at] == '=') /* exact */                                        \
			pf = move(filter::PackageFilter::NAME(filter.substr(at+1), neg));       \
		else if (filter[at] == ':') /* glob */                                    \
			pf = move(filter::PackageFilter::NAME##glob(filter.substr(at+1), neg)); \
		MAKE_PKGFILTER_REGEXPART(NAME)                                            \
		else {                                                                    \
			log(Error, "unknown " #NAME " filter: %s\n", filter.c_str());           \
			return false;                                                           \
		}                                                                         \
		if (!pf) {                                                                \
			log(Error, "failed to create " #NAME " filter: %s\n", filter.c_str());  \
			return false;                                                           \
		}                                                                         \
		pkg_filters.push_back(move(pf));                                          \
		return true;                                                              \
	}
	MAKE_PKGFILTER(group)
	MAKE_PKGFILTER(depends)
	MAKE_PKGFILTER(optdepends)
	MAKE_PKGFILTER(alldepends)
	MAKE_PKGFILTER(provides)
	MAKE_PKGFILTER(conflicts)
	MAKE_PKGFILTER(replaces)
# undef MAKE_PKGFILTER
# undef MAKE_PKGFILTER_REGEXPART
	else if (filter.compare(at, std::string::npos, "broken") == 0) {
		auto pf = filter::PackageFilter::broken(neg);
		if (!pf)
			return false;
		pkg_filters.push_back(move(pf));
		return true;
	}
#ifdef WITH_REGEX
# define MAKE_OBJFILTER_REGEXPART(NAME)                                       \
		else if (parse_regex())                                                   \
			pf = move(filter::ObjectFilter::NAME##regex(regex, true, icase, neg));
#else
# define MAKE_OBJFILTER_REGEXPART(NAME)
#endif
# define MAKE_OBJFILTER(NAME) \
	else if (filter.compare(at, 3+sizeof(#NAME)-1, "lib" #NAME) == 0) {             \
		at += 3+sizeof(#NAME)-1;                                                      \
		unique_ptr<filter::ObjectFilter> pf(nullptr);                                 \
		if (filter[at] == '=') /* exact */                                            \
			pf = move(filter::ObjectFilter::NAME(filter.substr(at+1), neg));       \
		else if (filter[at] == ':') /* glob */                                        \
			pf = move(filter::ObjectFilter::NAME##glob(filter.substr(at+1), neg)); \
		MAKE_OBJFILTER_REGEXPART(NAME)                                                \
		else {                                                                        \
			log(Error, "unknown lib" #NAME " filter: %s\n", filter.c_str());            \
			return false;                                                               \
		}                                                                             \
		if (!pf) {                                                                    \
			log(Error, "failed to create lib" #NAME " filter: %s\n", filter.c_str());   \
			return false;                                                               \
		}                                                                             \
		obj_filters.push_back(move(pf));                                              \
		return true;                                                                  \
	}

	MAKE_OBJFILTER(name)
	MAKE_OBJFILTER(depends)
	MAKE_OBJFILTER(path)

# undef MAKE_OBJFILTER
# undef MAKE_OBJFILTER_REGEXPART

	return false;
}
