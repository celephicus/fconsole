
/* Unit test framework adapted from minunit: http://www.jera.com/techinfo/jtns/jtn002.html
	There is no source file, it's all in the header.
	There is some customisation you can do before including this header, all are macros leading MINUNIT_...
*/

#ifndef MINUNIT_H__
#define MINUNIT_H__

#include "minunit_config.h"

// Holds minunit state. Define a symbol in one source file to instanciate.
typedef struct {
	char msg[MINUNIT_MESSAGE_BUFFER_LEN];
	unsigned tests_run, tests_fail;
} MinunitCtx;

extern MinunitCtx mu_ctx;
#ifdef MINUNIT_DECL_CTX
MinunitCtx mu_ctx;
#endif

// Helpers...
#define mu_mkstr2(s_) #s_
#define mu_mkstr(s_) mu_mkstr2(s_)

// Minunit API.
//

// Call at start to initialise.
void mu_init(void);

// User hooks must be defined.
const char* mu_test_setup(void);
void mu_test_teardown(void);

// Add a string to a staticbuffer, useful for building error messages. Buffer cleared at start of test.
char* mu_add_msg(const char* fmt, ...);

/* Run a test. Note the test function can take any number of arguments, but it must return a char*.
 	If the test_setup function fails then the test fails and the teardown function is NOT run.
 	Then the test function is called and the teardown function called i
  */
#define mu_run_test(_test) do {														\
	bool setup_ok;																	\
	mu_ctx.msg[0] = '\0'; mu_ctx.tests_run++;										\
	const char* test_result = mu_test_setup();										\
	setup_ok = !test_result;														\
	if (setup_ok) { printf("calling test...\n"); 									\
		test_result = (_test); 	}													\
	if (test_result) {																\
		mu_ctx.tests_fail++;														\
		printf(MINUNIT_DECL_STR("Fail: %s: %s\n"), mu_mkstr(_test), test_result);	\
	}																				\
	else																			\
		printf(MINUNIT_DECL_STR("Pass: %s\n"), mu_mkstr(_test));					\
	if (setup_ok) 																	\
		mu_test_teardown();															\
} while (0)

#define mu_assert_equal_str(val_, exp_) do { 																					\
	if (0 != strcmp((val_), (exp_))) 																							\
  		return mu_add_msg(MINUNIT_DECL_STR("%s != %s; expected `%s', got `%s'."), mu_mkstr(val_), mu_mkstr(exp_), exp_, val_); 	\
} while (0)
#define mu_assert_equal_int(val_, exp_) do { 																					\
	if ((int)(val_) != (int)(exp_)) 																							\
  		return mu_add_msg(MINUNIT_DECL_STR("%s != %s; expected `%d', got `%d'."), mu_mkstr(val_), mu_mkstr(exp_), exp_, val_); 	\
} while (0)

void mu_print_summary(void);

#define mu_rc() (!!mu_ctx.tests_fail)

#endif // MINUNIT_H__
