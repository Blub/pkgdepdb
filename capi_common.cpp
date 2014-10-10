#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "pkgdepdb.h"

#include "capi_algorithm.h"

extern "C" {

static char *pkgdepdb_err = NULL;
static char *pkgdepdb_old = NULL;

void pkgdepdb_init(void) {
}

void pkgdepdb_finalize(void) {
  free(pkgdepdb_err);
  free(pkgdepdb_old);
}

void pkgdepdb_clear_error(void) {
  free(pkgdepdb_err);
  free(pkgdepdb_old);
  pkgdepdb_err = NULL;
  pkgdepdb_old = NULL;
}

static void pkgdepdb_rotate_error(void) {
  free(pkgdepdb_old);
  pkgdepdb_old = pkgdepdb_err;
  pkgdepdb_err = NULL;
}

const char* pkgdepdb_error(void) {
  pkgdepdb_rotate_error();
  return pkgdepdb_old;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
void pkgdepdb_set_error(const char *fmt, ...) {
  va_list ap;
  int     len;

  pkgdepdb_rotate_error();

  va_start(ap, fmt);
  len = vprintf(fmt, ap);
  va_end(ap);

  pkgdepdb_err = (char*)malloc(len+1);
  va_start(ap, fmt);
  len = vsprintf(pkgdepdb_err, fmt, ap);
  va_end(ap);
  pkgdepdb_err[len] = 0;
}
#pragma clang diagnostic pop

} // extern "C"
