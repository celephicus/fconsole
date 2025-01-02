/* Unit test framework adapted from minunit: http://www.jera.com/techinfo/jtns/jtn002.html
	There is no source file, it's all in the header.
	There is some customisation you can do before including this header, all are macros leading MINUNIT_...
*/	

#ifndef MINUNIT_H__
#define MINUNIT_H__

#include "minunit_config.h"

// Helpers, not intended for calling directly.
extern char mu_msg[];
void mu_clear_msg(void);
char* mu_add_msg(const char* fmt, ...);

const char* mu_init_test_helper(void);
void mu_run_test_helper(const char* test_name, const char* test_result);
#define mu_mkstr(s_) #s_

extern int mu_tests_fail, mu_tests_run;

// Minunit API.

// Call at start to initialise.
void mu_init(void);

// User hooks must be defined.
const char* mu_test_setup(void);
void mu_test_teardown(void);

/* Run a test. Note the test function can take any number of arguments, but it must return a char*.
 * Implementation detail: we require that the test environment is setup before the user test function is run.
 * Hence the call to mu_init_test_helper(). */
#define mu_run_test(_test) mu_run_test_helper(mu_mkstr(_test), (mu_init_test_helper(), (_test)))

#define mu_assert_equal_str(val_, expected_) do { 																				\
	if (0 != strcmp((val_), (expected_))) 																						\
  		return mu_add_msg(MINUNIT_DECL_STR("%s != %s; expected `%s', got `%s'."), mu_mkstr(val_), mu_mkstr(expected_), expected_, val_); 	\
} while (0)
#define mu_assert_equal_int(val_, expected_) do { 																				\
	if ((int)(val_) != (int)(expected_)) 																						\
  		return mu_add_msg(MINUNIT_DECL_STR("%s != %s; expected `%d', got `%d'."), mu_mkstr(val_), mu_mkstr(expected_), expected_, val_); 	\
} while (0)

void mu_print_summary(void);

#endif // MINUNIT_H__
