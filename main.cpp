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
	{ "help",    no_argument, 0, 'h' },
	{ "version", no_argument, 0, 'v' },
	{ "install", no_argument, 0, 'i' },

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
	             );
	exit(x);
}

static void
version(int x)
{
	printf("readpkgelf " FULL_VERSION_STRING "\n");
	exit(x);
}

void db_commit_packages(std::vector<Package*> &&pkg);

int
main(int argc, char **argv)
{
	arg0 = argv[0];

	if (argc < 2)
		help(1);

	bool do_install = false;
	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hvi", long_opts, &opt_index);
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
	if (do_install) {
		printf("committing to database...\n");
		db_commit_packages(std::move(packages));
	}

	return 0;
}
