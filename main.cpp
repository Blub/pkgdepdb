#include <stdio.h>
#include <stdarg.h>

#include <memory>

#include <archive.h>
#include <archive_entry.h>

#include "main.h"

static int LogLevel = Warn;

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

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	if (argc != 2) {
		log(Error, "usage error\n");
		return 1;
	}

	Package package(argv[1]);
	if (!package)
		log(Error, "there was an error\n");
	return 0;
}
