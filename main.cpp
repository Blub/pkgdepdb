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
	{ "install", no_argument,       0, 'i' },
	{ "db",      required_argument, 0, 'd' },
	{ "info",    no_argument,       0, 'I' },
	{ "list",    no_argument,       0, 'L' },
	{ "missing", no_argument,       0, 'M' },
	{ "found",   no_argument,       0, 'F' },
	{ "pkgs",    no_argument,       0, 'P' },
	{ "verbose", no_argument,       0, 'v' },
	{ "rename",  required_argument, 0, 'n' },

	{ "ld-append",  required_argument, 0, -'A' },
	{ "ld-prepend", required_argument, 0, -'P' },
	{ "ld-delete",  required_argument, 0, -'D' },
	{ "ld-insert",  required_argument, 0, -'I' },
	{ "ld-clear",   no_argument,       0, -'C' },

	{ "relink",     no_argument,       0, -'R' },

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
	             );
	fprintf(out, "db management options:\n"
	             "  -d, --db=FILE      set the database file to commit to\n"
	             "  -i, --install      install packages to a dependency db\n"
	             "  -r, --remove       remove packages from the database\n"
	             );
	fprintf(out, "db query options:\n"
	             "  -I, --info         show general information about the db\n"
	             "  -L, --list         list packages and object files\n"
	             "  -M, --missing      show the 'missing' table\n"
	             "  -F, --found        show the 'found' table\n"
	             "  -P, --pkgs         show the installed packages\n"
	             "  -n, --rename=NAME  rename the database\n"
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
	exit(x);
}

static void
version(int x)
{
	printf("readpkgelf " FULL_VERSION_STRING "\n");
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
	}
	inline void operator=(const char *arg) {
		on = true;
		this->arg.push_back(arg);
	}
	inline void operator=(std::string &&arg) {
		on = true;
		this->arg.push_back(std::move(arg));
	}
};


int
main(int argc, char **argv)
{
	arg0 = argv[0];

	if (argc < 2)
		help(1);

	std::string  dbfile, newname;
	unsigned int verbose = 0;
	bool         do_install    = false;
	bool         do_delete     = false;
	bool         has_db        = false;
	bool         modified      = false;
	bool         show_info     = false;
	bool         show_list     = false;
	bool         show_missing  = false;
	bool         show_found    = false;
	bool         show_packages = false;
	bool         do_rename     = false;
	bool         do_relink     = false;
	bool oldmode = true;

	// library path options
	ArgArg ld_append (&oldmode),
	       ld_prepend(&oldmode),
	       ld_delete (&oldmode),
	       ld_clear  (&oldmode);
	std::vector<std::tuple<std::string,size_t>> ld_insert;

	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hird:ILMFPvn:", long_opts, &opt_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				help(0);
				break;
			case -'v':
				version(0);
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

			case 'v': ++verbose;            break;
			case 'i': oldmode = false; do_install    = true; break;
			case 'r': oldmode = false; do_delete     = true; break;
			case 'I': oldmode = false; show_info     = true; break;
			case 'L': oldmode = false; show_list     = true; break;
			case 'M': oldmode = false; show_missing  = true; break;
			case 'F': oldmode = false; show_found    = true; break;
			case 'P': oldmode = false; show_packages = true; break;

			case -'R': oldmode = false; do_relink = true; break;

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

			case ':':
			case '?':
				help(1);
				break;
		}
	}

	if (do_install && do_delete) {
		log(Error, "--install and --remove are mutually exclusive\n");
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
		while (optind < argc) {
			Package *package = Package::open(argv[optind++]);
			for (auto &obj : package->objects) {
				const char *objdir  = obj->dirname.c_str();
				const char *objname = obj->basename.c_str();
				for (auto &need : obj->needed) {
					printf("%s : %s/%s NEEDS %s\n",
					       package->name.c_str(),
					       objdir, objname,
					       need.c_str());
				}
			}
			delete package;
		}
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
		log(Message, "reading database\n");
		if (!db->read(dbfile)) {
			log(Error, "failed to read database\n");
			return 1;
		}
	}

	if (do_rename) {
		modified = true;
		db->name = newname;
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
		modified = db->ld_clear()             || modified;

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
		db->relink_all();
	}

	if (show_info)
		db->show_info();

	if (show_packages)
		db->show_packages();

	if (show_list)
		db->show_objects();

	if (show_missing)
		db->show_missing();

	if (show_found)
		db->show_found();

	if (modified && has_db) {
		log(Message, "writing new database\n");
		if (!db->store(dbfile))
			log(Error, "failed to write to the database\n");
	}

	return 0;
}
