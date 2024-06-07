#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#include "console.h"

// Test code from adapted from minunit: http://www.jera.com/techinfo/jtns/jtn002.html
static char mu_msg[200];
static void mu_clear_msg(void) { mu_msg[0] = '\0'; }
static char* mu_add_msg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsprintf(mu_msg + strlen(mu_msg), fmt, ap);
	return mu_msg;
}

int mu_tests_run, mu_tests_fail;
#define mu_mkstr(s_) #s_
#define mu_init() { mu_tests_run = mu_tests_fail = 0; }
#define mu_run_test(_test) do { 									\
	mu_clear_msg();													\
	mu_test_setup();	/* User setup hook. */						\
	mu_tests_run++; 												\
	const char* fail_msg = _test; 									\
	if (fail_msg) { 												\
		mu_tests_fail++;											\
		printf(PSTR("Fail: " mu_mkstr(_test) ": %s\n"), fail_msg); 	\
	} 																\
	mu_test_teardown();	/* User teardown hook. */					\
} while (0)

#define mu_assert_equal_str(val_, expected_) do { 																	\
	if (0 != strcmp((val_), (expected_))) 																			\
  		return mu_add_msg(PSTR("%s != %s; expected `%s', got `%s'."), mu_mkstr(val_), mu_mkstr(expected_), expected_, val_); 	\
} while (0)
#define mu_assert_equal_int(val_, expected_) do { 																	\
	if ((int)(val_) != (int)(expected_)) 																			\
  		return mu_add_msg(PSTR("%s != %s; expected `%d', got `%d'."), mu_mkstr(val_), mu_mkstr(expected_), expected_, val_); 	\
} while (0)

static void	mu_print_summary(void) {
	printf(PSTR("Tests run: %d, failed: %d.\n"), mu_tests_run, mu_tests_fail);
}

// Recogniser to support our tests.
enum {
	CONSOLE_RC_ERR_USER_EXIT = CONSOLE_RC_ERR_USER, // Exit from program
};

static bool console_cmds_user(char* cmd) {
	switch (console_hash(cmd)) {
		case /** + ( x1 x2 - x3) Add values: x3 = x1 + x2. **/ 0XB58E: console_binop(+); break;
		case /** - ( x1 x2 - x3) Subtract values: x3 = x1 - x2. **/ 0XB588: console_binop(-); break;
		case /** NEGATE ( d1 - d2) Negate signed value: d2 = -d1. **/ 0X7A79: console_unop(-); break;
		case /** # ( - ) Comment, rest of input ignored. **/ 0XB586: console_raise(CONSOLE_RC_STAT_IGN_EOL); break;
		case /** RAISE ( i - ) Raise value as exception. **/ 0X4069: console_raise((console_rc_t)console_u_pop()); break;
		case /** EXIT ( - ?) Exit console. **/ 0XC745: console_raise(CONSOLE_RC_ERR_USER_EXIT); break;
		case /** PICK (u - x) Copy stack item by index. **/ 0X13B4: console_u_tos() = console_u_get((console_small_uint_t)console_u_tos()+1); break;
		case /** OVER (x1 x2 - x1 x2 x1) Copy second stack item. **/ 0X398B: console_u_push(console_u_nos()); break;
		default: return false;
	}
	return true;
}

/* The number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
	can have potentially any hash value so could look like commands. */
static const console_recogniser_func RECOGNISERS[] PROGMEM = {
	console_r_number_decimal,
	console_r_number_hex,
	console_r_string,
	console_r_hex_string,
	console_cmds_builtin,
#ifdef CONSOLE_WANT_HELP
	console_cmds_help, 
#endif // CONSOLE_WANT_HELP
	console_cmds_user,
	NULL
};

// Test print routine, writes to string.
static char print_output_buf[100], *print_output_p;
static void print_output_init(void) { print_output_p = print_output_buf; }
static const char* print_output_get(void) { *print_output_p = '\0'; return print_output_buf; }
void consolePrint(console_small_uint_t opt, console_int_t x) {
	int nch = 0;
	switch (opt & 0x7f) {
		case CONSOLE_PRINT_NEWLINE:		nch = sprintf(print_output_p, "\n"); (void)x; opt |= CONSOLE_PRINT_NO_SEP; break;	// No separator.
		default:						(void)x; opt |= CONSOLE_PRINT_NO_SEP; break;						// Ignore, print nothing.
		case CONSOLE_PRINT_SIGNED:		nch = sprintf(print_output_p, "%ld", x); break;
		case CONSOLE_PRINT_UNSIGNED:	nch = sprintf(print_output_p, "+%lu", (console_uint_t)x); break;
		case CONSOLE_PRINT_HEX:			nch = sprintf(print_output_p, "$%016lX", (console_uint_t)x); break;
		case CONSOLE_PRINT_STR_P:		/* Fall through... */
		case CONSOLE_PRINT_STR:			nch = sprintf(print_output_p, "%s",(const char*)x); break;
		case CONSOLE_PRINT_CHAR:		nch = sprintf(print_output_p, "%c", (char)x); break;
	}
	print_output_p += nch;
	if (!(opt & CONSOLE_PRINT_NO_SEP))	nch = sprintf(print_output_p, "%c",' ');								// Print a space.
	print_output_p += nch;
}

static void mu_test_setup(void) {
	mu_msg[0] = '\0'; 										// Clear message buffer.
	print_output_init();
	consoleInit(RECOGNISERS);							// Setup console.
	consoleAcceptClear();								// NOT done by console Init. 
}
static void mu_test_teardown(void) {
	/* empty */
}

// Test test harness.
//
#if 0
static const char* check_test_harness_int(console_int_t x) { mu_assert_equal_int(x, 1234); return NULL; }
static const char* check_test_harness_str(const char* s) { mu_assert_equal_str(s, "az"); return NULL; }
#endif

// Test console output function.
static char* check_print(console_small_uint_t opt, console_int_t x, const char* output) {
	consolePrint(opt, x);
	mu_assert_equal_str(print_output_get(), output);	// Verify output string...
	return NULL;
}

static char* check_console(const char* input, const char* output, console_rc_t rc_expected, console_small_uint_t depth_expected, ...) {
	char inbuf[100];									// Copy input string as we are not meant to write to string literals.
	va_list ap;
	console_small_uint_t i;

	strcpy(inbuf, input);
	console_rc_t rc = consoleProcess(inbuf, NULL);		// Process input string.
	mu_assert_equal_int(rc, rc_expected);				// Verify return code...
	mu_assert_equal_str(print_output_get(), output);	// Verify output string...
	
	// Build error message for stack.
	mu_add_msg("Stack: [%d] ", console_u_depth());
	i = console_u_depth();
	while (i-- > 0)
		mu_add_msg("%" CONSOLE_FORMAT_INT " ", console_u_get(i)); 

	// Verify stack, starting with depth, then contents.
	if (console_u_depth() != depth_expected) 
		return mu_msg;

	va_start(ap, depth_expected);
	i = console_u_depth();
	while (i-- > 0) {
		if (console_u_get(i) != va_arg(ap, console_int_t)) 
			return mu_msg;
	}

	return NULL;
}

static char* check_hex_string(const char* input, console_small_uint_t len_expected, ...) {
	char inbuf[100];									// Copy input string as we are not meant to write to string literals.
	va_list ap;
	uint8_t* mem_ptr;
	console_small_uint_t i;

	strcpy(inbuf, input);
	console_rc_t rc = consoleProcess(inbuf, NULL);		// Process input string.
	mu_assert_equal_int(rc, CONSOLE_RC_OK);				// Verify return code...
	mu_assert_equal_str(print_output_get(), "");		// Verify no output string...
	mu_assert_equal_int(console_u_depth(), 1);

	// Build error message for memory.
	mem_ptr = (uint8_t*)console_u_tos();
	mu_add_msg("Hex string: ");
	i = *mem_ptr++;
	while (i-- > 0)
		mu_add_msg("%02X", *mem_ptr++); 

	// Verify memory...
	mem_ptr = (uint8_t*)console_u_tos();
	if (*mem_ptr++ != len_expected)
		return mu_msg;

	va_start(ap, len_expected);
	i = len_expected;
	while (i-- > 0) {
		if (*mem_ptr++ != va_arg(ap, console_int_t)) 
			return mu_msg;
	}

	return NULL;
}

static char* check_accept(uint8_t char_count, uint8_t len_expected, uint8_t rc_expected) {
	console_rc_t rc;
	
	// Checks will accept up to CONSOLE_INPUT_BUFFER_SIZE chars.
	char chk[100];
	char* cp = chk;
	for (uint8_t i = 0; i < char_count; i += 1) {
		rc = consoleAccept(*cp++ = (char)('!' + i));				// Call accept with a printable character.
		mu_assert_equal_int(CONSOLE_RC_STAT_ACC_PEND, rc);
	}
	*cp = '\0';

	rc = consoleAccept(CONSOLE_INPUT_NEWLINE_CHAR);
	mu_assert_equal_int(rc_expected, rc);
	if (CONSOLE_RC_OK == rc) {
		mu_assert_equal_int(strlen(consoleAcceptBuffer()), len_expected);				// Verify expected length of string in buffer
		mu_assert_equal_int(strncmp(consoleAcceptBuffer(), chk, len_expected), 0);		// Verify contents with data that was sent to consoleAccept().
	}
	return NULL;
}

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	printf(PSTR("Arduino Console Unit Tests: %u bit.\n"), (unsigned)(8 * sizeof(console_int_t)));

	mu_init();

	// Test test harness.
#if 0	
	mu_run_test(check_test_harness_int(1234));
	mu_run_test(check_test_harness_int(-1234));
	mu_run_test(check_test_harness_str("az"));
	mu_run_test(check_test_harness_str("a"));
#endif

	// Print routine.
	mu_run_test(check_print(CONSOLE_PRINT_NEWLINE, 						1235, 			"\n"));
	mu_run_test(check_print((console_small_uint_t)-1,					1235, 			""));			// No output.
	mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 						0, 				"0 "));
	mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED, 					0, 				"+0 "));

	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					32767, 			"32767 "));
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					-32768, 		"-32768 "));
		mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED,					(console_int_t)0xffff, 		"+65535 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX, 						(console_int_t)0xffff, 		"$FFFF "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX,						0, 				"$0000 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX|CONSOLE_PRINT_NO_SEP,	0, 				"$0000"));
	}
	else if (sizeof(console_int_t) == 4) {	
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					2147483647L, 	"2147483647 "));
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					-2147483648L, 	"-2147483648 "));
		mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED, 				(console_int_t)0xffffffff, 	"+4294967295 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX, 						(console_int_t)0xffffffff, 	"$FFFFFFFF "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX,						0, 				"$00000000 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX|CONSOLE_PRINT_NO_SEP,	0, 				"$00000000"));
	}
	else if (sizeof(console_int_t) == 8) {	
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					(console_int_t)9223372036854775807L, 	"9223372036854775807 "));
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					(console_int_t)(-9223372036854775807L-1), 	"-9223372036854775808 "));
		/* Why (-9223372036854775807L-1) instead of -9223372036854775808L? Because this is two tokens, a negate op and the constant.
			Which overflows a signed int, hence the error. Another solution would be to use LONG_MIN from limits.h. */
		mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED, 				(console_int_t)0xffffffffffffffffUL, 	"+18446744073709551615 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX, 						(console_int_t)0xffffffffffffffffUL, 	"$FFFFFFFFFFFFFFFF "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX,						0, 				"$0000000000000000 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX|CONSOLE_PRINT_NO_SEP,	0, 				"$0000000000000000"));
	}
	else
		mu_run_test("console_int_t not 16, 32 or 64 bit!");

	mu_run_test(check_print(CONSOLE_PRINT_STR, (console_int_t)"hello", "hello "));
	mu_run_test(check_print(CONSOLE_PRINT_STR_P, (console_int_t)PSTR("hello"), "hello "));
	mu_run_test(check_print(CONSOLE_PRINT_CHAR, 'x', "x "));

	mu_run_test(check_print(CONSOLE_PRINT_NEWLINE|CONSOLE_PRINT_NO_SEP,		1235, "\n"));  // NO_SEP has no effect. 
	mu_run_test(check_print((CONSOLE_PRINT_NO_SEP-1)|CONSOLE_PRINT_NO_SEP,	1235, ""));		// No output for bad format..
	mu_run_test(check_print(CONSOLE_PRINT_SIGNED|CONSOLE_PRINT_NO_SEP,		0, "0"));
	mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED|CONSOLE_PRINT_NO_SEP,	0, "+0"));
	mu_run_test(check_print(CONSOLE_PRINT_STR|CONSOLE_PRINT_NO_SEP,			(console_int_t)"hello", "hello"));
	mu_run_test(check_print(CONSOLE_PRINT_STR_P|CONSOLE_PRINT_NO_SEP,		(console_int_t)PSTR("hello"), "hello"));
	mu_run_test(check_print(CONSOLE_PRINT_CHAR|CONSOLE_PRINT_NO_SEP,		(console_int_t)'x', "x"));

	// Test console...
	mu_run_test(check_console("", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console(" ", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console("\t", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console("foo", "",					CONSOLE_RC_ERR_BAD_CMD,	0));


	// Check decimal number parser.
	mu_run_test(check_console("+a", "",						CONSOLE_RC_ERR_BAD_CMD,	0));	// We could have a command `+a' if we wanted. It's just not a number.
	mu_run_test(check_console("-f", "",						CONSOLE_RC_ERR_BAD_CMD,	0));	// Likewise `-f' could be a command, but it's not a number. 
	mu_run_test(check_console("0", "",						CONSOLE_RC_OK,				1, (console_int_t)0));
	mu_run_test(check_console("+0", "",						CONSOLE_RC_OK,				1, (console_int_t)0));
	mu_run_test(check_console("1", "",						CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("1a", "",						CONSOLE_RC_ERR_BAD_CMD, 0));		// Flagged as unknown command, even though it's really a bad base. We could have a command `1a'. 

	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_console("32767", "",				CONSOLE_RC_OK,				1, 32767));
		mu_run_test(check_console("-32768", "",				CONSOLE_RC_OK,				1, -32768));
		mu_run_test(check_console("32768", "",				CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("-32769", "",				CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("+32768", "",				CONSOLE_RC_OK,				1, 32768));
		mu_run_test(check_console("+65535", "",				CONSOLE_RC_OK,				1, 65535));
		mu_run_test(check_console("+65536", "",				CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else if (sizeof(console_int_t) == 4) {
		mu_run_test(check_console("2147483647", "",			CONSOLE_RC_OK,				1, 2147483647L));
		mu_run_test(check_console("-2147483648", "",		CONSOLE_RC_OK,				1, -2147483648L));
		mu_run_test(check_console("2147483648", "",			CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("-2147483649", "",		CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("+2147483647", "",		CONSOLE_RC_OK,				1, 2147483647L));
		mu_run_test(check_console("+4294967295", "",		CONSOLE_RC_OK,				1, 4294967295));
		mu_run_test(check_console("+4294967296", "",		CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else if (sizeof(console_int_t) == 8) {
		mu_run_test(check_console("9223372036854775807", "",			CONSOLE_RC_OK,				1, 9223372036854775807L));
		mu_run_test(check_console("-9223372036854775808", "",		CONSOLE_RC_OK,				1, (-9223372036854775807L-1)));
		mu_run_test(check_console("9223372036854775808", "",			CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("-9223372036854775809", "",		CONSOLE_RC_ERR_NUM_OVF,	0));
		mu_run_test(check_console("+9223372036854775807", "",		CONSOLE_RC_OK,				1, 9223372036854775807UL));
		mu_run_test(check_console("+18446744073709551615", "",		CONSOLE_RC_OK,				1, 18446744073709551615UL));
		mu_run_test(check_console("+18446744073709551616", "",		CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else
		mu_run_test("console_int_t not 16, 32 or 64 bit!");

	// Check hex number parser.
	mu_run_test(check_console("$g", "",						CONSOLE_RC_ERR_BAD_CMD,	0));	// We could have this command if we wanted. It's just not a number.
	mu_run_test(check_console("$", "",						CONSOLE_RC_ERR_BAD_CMD,	0));
	mu_run_test(check_console("$1", "",						CONSOLE_RC_OK,				1, (console_int_t)1));
	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_console("$FFFF", "",					CONSOLE_RC_OK,				1, 0xffff));
		mu_run_test(check_console("$10000", "",					CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else if (sizeof(console_int_t) == 4) {
		mu_run_test(check_console("$FFFFFFFF", "",					CONSOLE_RC_OK,				1, 0xffffffff));
		mu_run_test(check_console("$100000000", "",					CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else if (sizeof(console_int_t) == 8) {
		mu_run_test(check_console("$FFFFFFFFFFFFFFFF", "",					CONSOLE_RC_OK,				1, 0xffffffffffffffff));
		mu_run_test(check_console("$10000000000000000", "",					CONSOLE_RC_ERR_NUM_OVF,	0));
	}
	else
		mu_run_test("console_int_t not 16, 32 or 64 bit!");

	// Check string parser & string printer.
	mu_run_test(check_console(".\"", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("\" .\"", " ",				CONSOLE_RC_OK,				0));			// Zero length string.
	mu_run_test(check_console("\"\\\\ .\"", "\\ ",			CONSOLE_RC_OK,				0));			// Single backslash.
	mu_run_test(check_console("\"q .\"", "q ",				CONSOLE_RC_OK,				0));
	mu_run_test(check_console("\"q\\n\\r\\ .\"", "q\n\r ",	CONSOLE_RC_OK,				0));			// Trailing \ ignored.
	mu_run_test(check_console("\"\\1f .\"", "\x1f ",		CONSOLE_RC_OK,				0));			// Single character in hex `1f'. 
	mu_run_test(check_console("\"\\1g .\"", "1g ",			CONSOLE_RC_OK,				0));			// Bad hex escape.

	// Check hex string 
	mu_run_test(check_console("&", "",						CONSOLE_RC_ERR_BAD_CMD,	0));				// Bad hex string.
	mu_run_test(check_console("&1", "",						CONSOLE_RC_ERR_BAD_CMD,	0));				// Bad hex string.
	mu_run_test(check_console("&1g", "",					CONSOLE_RC_ERR_BAD_CMD,	0));				// Bad hex string.
	mu_run_test(check_console("&ff1g", "",					CONSOLE_RC_ERR_BAD_CMD,	0));				// Bad hex string.
	mu_run_test(check_hex_string("&1a", 1, 0x1a));
	mu_run_test(check_hex_string("&1a00", 2, 0x1a, 0x00));

	// Number printing.
	mu_run_test(check_console(".", "",						CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("0 .", "0 ",					CONSOLE_RC_OK,				0));
	mu_run_test(check_console("U.", "",						CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("0 U.", "+0 ",				CONSOLE_RC_OK,				0));
	mu_run_test(check_console("$.", "",						CONSOLE_RC_ERR_DSTK_UNF,	0));
	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_console("32767 .", "32767 ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("-32768 .", "-32768 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("+65535 U.", "+65535 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("0 $.", "$0000 ",				CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$000F $.", "$000F ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$00FF $.", "$00FF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$0FFF $.", "$0FFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$FFFF $.", "$FFFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$AF19 $.", "$AF19 ",			CONSOLE_RC_OK,				0));
	}
	else if (sizeof(console_int_t) == 4) {
		mu_run_test(check_console("2147483647 .", "2147483647 ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("-2147483648 .", "-2147483648 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("+4294967295 U.", "+4294967295 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("0 $.", "$00000000 ",				CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$000F $.", "$0000000F ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$00FF $.", "$000000FF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$0FFF $.", "$00000FFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$FFFF $.", "$0000FFFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$DE32AF19 $.", "$DE32AF19 ",			CONSOLE_RC_OK,				0));
	}
	else if (sizeof(console_int_t) == 8) {
		mu_run_test(check_console("9223372036854775807 .", "9223372036854775807 ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("-9223372036854775808 .", "-9223372036854775808 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("+18446744073709551615 U.", "+18446744073709551615 ",		CONSOLE_RC_OK,				0));
		mu_run_test(check_console("0 $.", "$0000000000000000 ",				CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$000F $.", "$000000000000000F ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$00FF $.", "$00000000000000FF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$0FFF $.", "$0000000000000FFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$FFFF $.", "$000000000000FFFF ",			CONSOLE_RC_OK,				0));
		mu_run_test(check_console("$DE32AF19DE32AF19 $.", "$DE32AF19DE32AF19 ",			CONSOLE_RC_OK,				0));
	}
	else
		mu_run_test("console_int_t not 16 or 32 bit!");

	// Stack over/under flow.
	mu_run_test(check_console("DROP", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("1 2 3 4 5 ", "",		CONSOLE_RC_ERR_DSTK_OVF,	4, (console_int_t)1, (console_int_t)2, (console_int_t)3, (console_int_t)4));

	// Commands
	mu_run_test(check_console("1 2 DROP", "",				CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("1 2 drop", "",				CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("\"HASH HASH", "",			CONSOLE_RC_OK,				1, (console_int_t)0x90b7));
	mu_run_test(check_console("\"hash HASH", "",			CONSOLE_RC_OK,				1, (console_int_t)0x90b7));
	mu_run_test(check_console("1 2 CLEAR", "",				CONSOLE_RC_OK,				0));
	mu_run_test(check_console("DEPTH", "",					CONSOLE_RC_OK,				1, (console_int_t)0));
	mu_run_test(check_console("123 DEPTH", "",				CONSOLE_RC_OK,				2, (console_int_t)123, (console_int_t)1));
	mu_run_test(check_console("1 1 1 1 DEPTH", "",			CONSOLE_RC_ERR_DSTK_OVF,	4, (console_int_t)1, (console_int_t)1, (console_int_t)1, (console_int_t)1));
	mu_run_test(check_console("PICK", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("0 PICK", "",					CONSOLE_RC_ERR_BAD_IDX,		1, (console_int_t)0));
	mu_run_test(check_console("1234 0 PICK", "",			CONSOLE_RC_OK,				2, (console_int_t)1234, (console_int_t)1234));
	mu_run_test(check_console("2345 1234 1 PICK", "",		CONSOLE_RC_OK,				3, (console_int_t)2345, (console_int_t)1234, (console_int_t)2345));
	mu_run_test(check_console("2345 1234 2 PICK", "",		CONSOLE_RC_ERR_BAD_IDX,		3, (console_int_t)2345, (console_int_t)1234, (console_int_t)2));
	mu_run_test(check_console("OVER", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("1234 OVER", "",				CONSOLE_RC_ERR_DSTK_UNF,	1, (console_int_t)1234));
	mu_run_test(check_console("1234 2345 OVER", "",			CONSOLE_RC_OK,				3, (console_int_t)1234, (console_int_t)2345, (console_int_t)1234));
	mu_run_test(check_console("1 1 1 1 OVER", "",			CONSOLE_RC_ERR_DSTK_OVF,	4, (console_int_t)1, (console_int_t)1, (console_int_t)1, (console_int_t)1));

	// Arithmetic operators.
	mu_run_test(check_console("1 +", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("+", "",						CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("1 2 +", "",					CONSOLE_RC_OK,				1, (console_int_t)3));
	mu_run_test(check_console("1 -", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("-", "",						CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("1 2 -", "",					CONSOLE_RC_OK,				1, (console_int_t)-1));
	mu_run_test(check_console("NEGATE", "",					CONSOLE_RC_ERR_DSTK_UNF,	0));
	mu_run_test(check_console("1 NEGATE", "",				CONSOLE_RC_OK,				1, (console_int_t)-1));
	
	// Comments & raise.
	mu_run_test(check_console("EXIT", "",					CONSOLE_RC_ERR_USER_EXIT,	0));
	mu_run_test(check_console("127 RAISE", "",				127,						0));
	mu_run_test(check_console("1 2 # 3 4", "",				CONSOLE_RC_OK,				2, (console_int_t)1, (console_int_t)2));

	// Test Accept
	mu_run_test(check_accept(0,								0,								CONSOLE_RC_OK));
	mu_run_test(check_accept(1,								1,								CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+1,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_ERR_ACC_OVF));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+2,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_ERR_ACC_OVF));

	mu_print_summary();

	return mu_tests_fail;
}


