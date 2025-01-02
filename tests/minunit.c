#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "minunit.h"

char mu_msg[MINUNIT_MESSAGE_BUFFER_LEN];
int mu_tests_run, mu_tests_fail;

void mu_clear_msg(void) { mu_msg[0] = '\0'; }

char* mu_add_msg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsprintf(mu_msg + strlen(mu_msg), fmt, ap);
	return mu_msg;
}

// Do test environment setup immediately prior to calling user test function. 
const char* mu_init_test_helper(void) {
	mu_clear_msg();													
	mu_tests_run++; 
	return 	mu_test_setup();
}

// Process results after user test function is run. 
void mu_run_test_helper(const char* test_name, const char* test_result) {
	if (test_result) { 
		mu_tests_fail++;									
		printf(MINUNIT_DECL_STR("Fail: %s: %s\n"), test_name, test_result);
	} 					
	mu_test_teardown();	
} 

void mu_init(void) { 
	mu_tests_run = mu_tests_fail = 0; 
}
void mu_print_summary(void) {
	printf(MINUNIT_DECL_STR("Tests run: %d, failed: %d.\n"), mu_tests_run, mu_tests_fail);
}
