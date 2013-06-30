#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>

#include <memory>

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

	{ 0, 0, 0, 0 }
};

static void
help(int x)
{
	FILE *out = x ? stderr : stdout;
	fprintf(out, "usage: %s [options] packages...\n", arg0);
	fprintf(out, "options:\n"
	             "  --help       show this message\n"
	             "  --version    show version info\n");
	exit(x);
}

static void
version(int x)
{
	printf("readpkgelf v0.0\n");
	exit(x);
}

int
main(int argc, char **argv)
{
	arg0 = argv[0];

	if (argc < 2)
		help(1);

	for (;;) {
		int opt_index = 0;
		int c = getopt_long(argc, argv, "hv", long_opts, &opt_index);
		if (c == -1)
			break;
		switch (c) {
			case 'h':
				help(0);
				break;
			case 'v':
				version(0);
				break;
			case ':':
			case '?':
				help(1);
				break;
		}
	}

	while (optind < argc) {
		Package package(argv[optind]);
		if (!package)
			log(Error, "error reading package %s\n", argv[optind]);
		package.show_needed();
		++optind;
	}

	return 0;
}
