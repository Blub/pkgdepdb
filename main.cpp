#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

static int LogLevel = Warn;
static const char *arg0 = 0;

void
log(int level, const char *msg, ...)
{
	if (level < LogLevel)
		return;
	va_list ap;
	va_start(ap, msg);
	vfprintf(stderr, msg, ap);
	va_end(ap);
}

static struct option long_opts[] = {
	{ "help",    no_argument,       0, 'h' },
	{ "version", no_argument,       0, 'v' },
	{ "install", no_argument,       0, 'i' },
	{ "db",      required_argument, 0, 'd' },

	{ 0, 0, 0, 0 }
};

static void
help(int x)
{
	FILE *out = x ? stderr : stdout;
	fprintf(out, "usage: %s [options] packages...\n", arg0);
	fprintf(out, "options:\n"
	             "  --help          show this message\n"
	             "  --version       show version info\n"
	             "  -i, --install   install packages to a dependency db\n"
	             "  -d, --db=FILE   set the database file to commit to\n"
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

	bool do_install = false;
	bool has_db = false;
	std::string dbfile;
	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hvid:", long_opts, &opt_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				help(0);
				break;
			case 'v':
				version(0);
				break;
			case 'i':
				do_install = true;
				break;
			case 'd':
				has_db = true;
				dbfile = optarg;
				break;
			case ':':
			case '?':
				help(1);
				break;
		}
	}

	std::vector<Package*> packages;

	if (do_install)
		printf("loading packages...\n");

	while (optind < argc) {
		if (do_install)
			printf(" ... %s\n", argv[optind]);

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

	printf("packages loaded...\n");
	do { // because goto sucks

		if (do_install) {
			DB db;
			if (has_db) {
				printf("reading old database...\n");
				if (!db.read(dbfile)) {
					log(Error, "failed to read database\n");
					break; // because goto sucks
				}
			}
			printf("committing to database...\n");
			for (auto pkg : packages) {
				if (!db.install_package(std::move(pkg))) {
					printf("failed to commit package %s to database\n", pkg->name.c_str());
					break;
				}
			}
			db.show();
			if (has_db) {
				printf("writing new database...\n");
				if (!db.store(dbfile)) {
					log(Error, "failed to write to the database\n");
				}
			}
		}
	} while(0);

	return 0;
}
