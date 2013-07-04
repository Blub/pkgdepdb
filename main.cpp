#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>

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
	{ "list",    no_argument,       0, 'L' },
	{ "missing", no_argument,       0, 'M' },
	{ "found",   no_argument,       0, 'F' },
	{ "verbose", no_argument,       0, 'v' },

	{ 0, 0, 0, 0 }
};

static void
help(int x)
{
	FILE *out = x ? stderr : stdout;
	fprintf(out, "usage: %s [options] packages...\n", arg0);
	fprintf(out, "options:\n"
	             "  -h, --help      show this message\n"
	             "  --version       show version info\n"
	             "  -i, --install   install packages to a dependency db\n"
	             "  -d, --db=FILE   set the database file to commit to\n"
	             "  -v, --verbose   print more information\n"
	             "  -L, --list      list packages and object files\n"
	             "  -M, --missing   show the 'missing' table\n"
	             "  -F, --found     show the 'found' table\n"
	             );
	exit(x);
}

static void
version(int x)
{
	printf("readpkgelf " FULL_VERSION_STRING "\n");
	exit(x);
}

int
main(int argc, char **argv)
{
	arg0 = argv[0];

	if (argc < 2)
		help(1);

	std::string  dbfile;
	unsigned int verbose = 0;
	bool         do_install   = false;
	bool         has_db       = false;
	bool         modified     = false;
	bool         show_list    = false;
	bool         show_missing = false;
	bool         show_found   = false;
	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hid:LMFv", long_opts, &opt_index);
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
				has_db = true;
				dbfile = optarg;
				break;

			case 'v': ++verbose;           break;
			case 'i': do_install   = true; break;
			case 'L': show_list    = true; break;
			case 'M': show_missing = true; break;
			case 'F': show_found   = true; break;

			case ':':
			case '?':
				help(1);
				break;
		}
	}

	std::vector<Package*> packages;

	if (optind < argc) {
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

		log(Message, "packages loaded...\n");
	}

	std::unique_ptr<DB> db(new DB);
	if (has_db) {
		log(Message, "reading old database\n");
		if (!db->read(dbfile)) {
			log(Error, "failed to read old database\n");
			return 1;
		}
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

	if (show_list)
		db->show();

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
