#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "console.h"
#include "minunit.h"

#pragma GCC diagnostic ignored "-Wunused-function"

static bool fail_setup = true;
const char* mu_test_setup(void) {
	const char* res = fail_setup ? "failed" : NULL;
	printf("In setup() %s\n", res);
	fail_setup = false;
	return res;
}
void mu_test_teardown(void) {
	printf("In teardown()\n");
}

static const char* check_test_harness_int(console_int_t x) { mu_assert_equal_int(x, 1234); return NULL; }
static const char* check_test_harness_str(const char* s) { mu_assert_equal_str(s, "az"); return NULL; }

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	printf("Checking minunit...\n");

	mu_init();

	mu_run_test(check_test_harness_int(1234));
	mu_run_test(check_test_harness_int(1234));
	mu_run_test(check_test_harness_int(-1234));
	mu_run_test(check_test_harness_str("az"));
	mu_run_test(check_test_harness_str("a"));
	mu_print_summary();

	return mu_rc();
}
