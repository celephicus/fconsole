#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#define MINUNIT_DECL_CTX
#include "minunit.h"


char* mu_add_msg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsprintf(mu_ctx.msg + strlen(mu_ctx.msg), fmt, ap);
	return mu_ctx.msg;
}

void mu_init(void) {
	mu_ctx.tests_run = mu_ctx.tests_fail = 0;
}
void mu_print_summary(void) {
	printf(MINUNIT_DECL_STR("Tests run: %d, failed: %d.\n"), mu_ctx.tests_run, mu_ctx.tests_fail);
}
