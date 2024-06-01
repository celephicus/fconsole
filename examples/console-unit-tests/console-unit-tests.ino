#include <fconsole.h>
#include <console-internals.h>

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

static bool console_cmds_user(char* cmd) {
	switch (console_hash(cmd)) {
		case /** + **/ 0XB58E: console_binop(+); break;
		case /** - **/ 0XB588: console_binop(-); break;
		case /** NEGATE **/ 0X7A79: console_unop(-); break;
		case /** # **/ 0XB586: console_raise(CONSOLE_RC_STATUS_IGNORE_TO_EOL); break;
		case /** LED **/ 0XDC88: digitalWrite(13, !!console_u_pop()); break;
		default: return false;
	}
	return true;
}

// Fake stream that writes to a static buffer.
class StaticBufferStream : public Stream {
public:
    StaticBufferStream(size_t sz) : _size(sz), _pos(0) { _buf = new char[sz]; }

	virtual int available() { return _size - _pos; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() { };
    virtual size_t write(uint8_t c) { _buf[_pos++] = (char)c; return 1; }
	char* get() { _buf[_pos] = '\0'; return _buf; }
	void clear() { _pos = 0; }
	
private:
    size_t _size;
    size_t _pos;
    char* _buf;
};

// Our console outputs to a string now.
StaticBufferStream o_stream(80);

static char msg[100];
char* mk_msg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsprintf_P(msg, fmt, ap);
	return msg;
}

void setup() {
	FConsole.begin(console_cmds_user, o_stream);	// Use fake stream object for console print function.  Calls consoleInit().

	Serial.begin(115200);
	Serial.println();
	Serial.println(mk_msg(PSTR("Arduino Console Unit Tests: %u bit."), 8 * sizeof(console_int_t)));
}

// Test code from adapted from minunit: http://www.jera.com/techinfo/jtns/jtn002.html
// It is a real mess, uses far too much memory, constant strings are in RAM rather than Progmem, etc.
int tests_run, tests_fail;
#define mkstr(s_) #s_
#define mu_init() { tests_run = tests_fail = 0; }
#define mu_run_test(_test) do { tests_run++; const char* msg = _test; if (msg) { Serial.print("Fail: " mkstr(_test) ": "); Serial.println(msg); } } while (0)

#define mu_assert_equal_str(val_, expected_) do { if (0 != strcmp((val_), (expected_))) { tests_fail++; return mk_msg(PSTR("%s != %s; expected `%s', got `%s'"), mkstr(val_), mkstr(expected_), expected_, val_); } } while (0)
#define mu_assert_equal_int(val_, expected_) do { if ((int)(val_) != (int)(expected_))  { tests_fail++; return mk_msg(PSTR("%s != %s; expected `%d', got `%d'"), mkstr(val_), mkstr(expected_), expected_, val_); } } while (0)

char* check_print(uint8_t opt, console_int_t x, const char* output) {
	o_stream.clear();									// Clear output string of contents from previous test.. 
	consolePrint(opt, x);
	mu_assert_equal_str(o_stream.get(), output);	// Verify output string...
	return NULL;
}

char* check_console(const char* input, const char* output, console_rc_t rc_expected, int8_t depth_expected, ...) {
	char inbuf[32];								// Copy input string as we are not meant to write to string literals.
	strcpy(inbuf, input);
	o_stream.clear();									// Clear output string of contents from previous test.. 
	consoleReset();								// Clear stack. 
	console_rc_t rc = consoleProcess(inbuf);	// Process input string.
	mu_assert_equal_int(rc, rc_expected);		// Verify return code...
	mu_assert_equal_str(o_stream.get(), output);	// Verify output string...
	
	// Verify stack, starting with depth, then contents.
	mu_assert_equal_int(consoleStackDepth(), depth_expected); 
	va_list ap;
	va_start(ap, depth_expected);
	while (depth_expected-- > 0) {
		if (consoleStackPick(depth_expected) != va_arg(ap, console_int_t)) { 
			sprintf(msg, " stack: "); 
			for (uint8_t i = 0; i < consoleStackDepth(); i += 1) 
				sprintf(msg+strlen(msg), "%" CONSOLE_FORMAT_INT " ", consoleStackPick(i)); 
			tests_fail++;
			return msg;
		}
	}
	return NULL;
}

char* check_accept(uint8_t char_count, uint8_t len_expected, uint8_t rc_expected) {
	consoleAcceptClear();
	console_rc_t rc;
	
	// Checks will accept up to CONSOLE_INPUT_BUFFER_SIZE chars.
	char chk[100];
	char* cp = chk;
	for (uint8_t i = 0; i < char_count; i += 1) {
		rc = consoleAccept(*cp++ = ('!' + i));				// Call accept with a printable character.
		mu_assert_equal_int(CONSOLE_RC_STATUS_ACCEPT_PENDING, rc);
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

// Since I had a Pro Mini handy all tests won't fit in the memory, so they have to be tested in batches.
// If you have a bigger processor such as an ArduinoMega then you can do the lot. 
#define TEST_BATCH 0

void loop() {
	mu_init();
#if (TEST_BATCH == 0) || (TEST_BATCH == 1)
	// Print routine.
	mu_run_test(check_print(CONSOLE_PRINT_NEWLINE, 						1235, 			CONSOLE_OUTPUT_NEWLINE_STR));
	mu_run_test(check_print(-1, 										1235, 			""));			// No output.
	mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 						0, 				"0 "));
	mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED, 					0, 				"+0 "));

	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					32767, 			"32767 "));
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					-32768, 		"-32768 "));
		mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED,					0xffff, 		"+65535 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX, 						0xffff, 		"$FFFF "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX,						0, 				"$0000 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX|CONSOLE_PRINT_NO_SEP,	0, 				"$0000"));
	}
	else if (sizeof(console_int_t) == 4) {	
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					2147483647L, 	"2147483647 "));
		mu_run_test(check_print(CONSOLE_PRINT_SIGNED, 					-2147483648L, 	"-2147483648 "));
		mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED, 				0xffffffff, 	"+4294967295 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX, 						0xffffffff, 	"$FFFFFFFF "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX,						0, 				"$00000000 "));
		mu_run_test(check_print(CONSOLE_PRINT_HEX|CONSOLE_PRINT_NO_SEP,	0, 				"$00000000"));
	}
	else
		mu_run_test("console_int_t not 16 or 32 bit!");

	mu_run_test(check_print(CONSOLE_PRINT_STR, (console_int_t)"hello", "hello "));
	mu_run_test(check_print(CONSOLE_PRINT_STR_P, (console_int_t)PSTR("hello"), "hello "));
	mu_run_test(check_print(CONSOLE_PRINT_CHAR, 'x', "x "));

	mu_run_test(check_print(CONSOLE_PRINT_NEWLINE|CONSOLE_PRINT_NO_SEP,		1235, CONSOLE_OUTPUT_NEWLINE_STR));  // NO_SEP has no effect. 
	mu_run_test(check_print((CONSOLE_PRINT_NO_SEP-1)|CONSOLE_PRINT_NO_SEP,	1235, ""));			// No output.
	mu_run_test(check_print(CONSOLE_PRINT_SIGNED|CONSOLE_PRINT_NO_SEP,		0, "0"));
	mu_run_test(check_print(CONSOLE_PRINT_UNSIGNED|CONSOLE_PRINT_NO_SEP,	0, "+0"));
	mu_run_test(check_print(CONSOLE_PRINT_STR|CONSOLE_PRINT_NO_SEP,			(console_int_t)"hello", "hello"));
	mu_run_test(check_print(CONSOLE_PRINT_STR_P|CONSOLE_PRINT_NO_SEP,		(console_int_t)PSTR("hello"), "hello"));
	mu_run_test(check_print(CONSOLE_PRINT_CHAR|CONSOLE_PRINT_NO_SEP,		(console_int_t)'x', "x"));
#endif

#if (TEST_BATCH == 0) || (TEST_BATCH == 2)	// Command parsing
	mu_run_test(check_console("", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console(" ", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console("\t", "",						CONSOLE_RC_OK,				0));
	mu_run_test(check_console("foo", "",					CONSOLE_RC_ERR_UNKNOWN_COMMAND,	0));
	
	// Check decimal number parser.
	mu_run_test(check_console("+a", "",						CONSOLE_RC_ERR_UNKNOWN_COMMAND,	0));	// We could have a command `+a' if we wanted. It's just not a number.
	mu_run_test(check_console("-f", "",						CONSOLE_RC_ERR_UNKNOWN_COMMAND,	0));	// Likewise `-f' could be a command, but it's not a number. 
	mu_run_test(check_console("0", "",						CONSOLE_RC_OK,				1, (console_int_t)0));
	mu_run_test(check_console("+0", "",						CONSOLE_RC_OK,				1, (console_int_t)0));
	mu_run_test(check_console("1", "",						CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("1a", "",						CONSOLE_RC_ERR_UNKNOWN_COMMAND, 0));		// Flagged as unknown command, even though it's really a bad base. We could have a command `1a'. 
	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_console("32767", "",				CONSOLE_RC_OK,				1, 32767));
		mu_run_test(check_console("-32768", "",				CONSOLE_RC_OK,				1, -32768));
		mu_run_test(check_console("32768", "",				CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
		mu_run_test(check_console("-32769", "",				CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
		mu_run_test(check_console("+32768", "",				CONSOLE_RC_OK,				1, 32768));
		mu_run_test(check_console("+65535", "",				CONSOLE_RC_OK,				1, 65535));
		mu_run_test(check_console("+65536", "",				CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
	}
	else if (sizeof(console_int_t) == 4) {
		mu_run_test(check_console("2147483647", "",			CONSOLE_RC_OK,				1, 2147483647L));
		mu_run_test(check_console("-2147483648", "",		CONSOLE_RC_OK,				1, -2147483648L));
		mu_run_test(check_console("2147483648", "",			CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
		mu_run_test(check_console("-2147483649", "",		CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
		mu_run_test(check_console("+2147483647", "",		CONSOLE_RC_OK,				1, 2147483647L));
		mu_run_test(check_console("+4294967295", "",		CONSOLE_RC_OK,				1, 4294967295));
		mu_run_test(check_console("+4294967296", "",		CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
	}
	else
		mu_run_test("console_int_t not 16 or 32 bit!");
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 3)
	// Check hex number parser.
	mu_run_test(check_console("$g", "",						CONSOLE_RC_ERR_UNKNOWN_COMMAND,	0));	// We could have this command if we wanted. It's just not a number.
	mu_run_test(check_console("$", "",						CONSOLE_RC_ERR_UNKNOWN_COMMAND,	0));
	mu_run_test(check_console("$1", "",						CONSOLE_RC_OK,				1, (console_int_t)1));
	if (sizeof(console_int_t) == 2) {
		mu_run_test(check_console("$FFFF", "",					CONSOLE_RC_OK,				1, 0xffff));
		mu_run_test(check_console("$10000", "",					CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
	}
	else if (sizeof(console_int_t) == 4) {
		mu_run_test(check_console("$FFFFFFFF", "",					CONSOLE_RC_OK,				1, 0xffffffff));
		mu_run_test(check_console("$100000000", "",					CONSOLE_RC_ERR_NUMBER_OVERFLOW,	0));
	}
	else
		mu_run_test("console_int_t not 16 or 32 bit!");
	
	// Check string parser & string printer.
	mu_run_test(check_console(".\"", "",					CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("\" .\"", " ",				CONSOLE_RC_OK,				0));			// Zero length string.
	mu_run_test(check_console("\"\\\\ .\"", "\\ ",			CONSOLE_RC_OK,				0));			// Single backslash.
	mu_run_test(check_console("\"q .\"", "q ",				CONSOLE_RC_OK,				0));
	mu_run_test(check_console("\"q\\n\\r\\ .\"", "q\n\r  ",	CONSOLE_RC_OK,				0));
	mu_run_test(check_console("\"\\1f .\"", "\x1f ",		CONSOLE_RC_OK,				0));			// Single character in hex `1f'. 
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 4)
	// Number printing.
	mu_run_test(check_console(".", "",						CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("0 .", "0 ",					CONSOLE_RC_OK,				0));
	mu_run_test(check_console("U.", "",						CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("0 U.", "+0 ",				CONSOLE_RC_OK,				0));
	mu_run_test(check_console("$.", "",						CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
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
	else
		mu_run_test("console_int_t not 16 or 32 bit!");

	// Stack over/under flow.
	mu_run_test(check_console("DROP", "",					CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 3 4 5 6 7 8 9 ", "",		CONSOLE_RC_ERR_DSTACK_OVERFLOW,	8, (console_int_t)1, (console_int_t)2, (console_int_t)3, (console_int_t)4, (console_int_t)5, (console_int_t)6, (console_int_t)7, (console_int_t)8));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 5)
	// Commands
	mu_run_test(check_console("1 2 DROP", "",				CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("1 2 drop", "",				CONSOLE_RC_OK,				1, (console_int_t)1));
	mu_run_test(check_console("\"HASH HASH", "",			CONSOLE_RC_OK,				1, (console_int_t)0x90b7));
	mu_run_test(check_console("\"hash HASH", "",			CONSOLE_RC_OK,				1, (console_int_t)0x90b7));
	mu_run_test(check_console("1 2 CLEAR", "",				CONSOLE_RC_OK,				0));

	// User commands.
	mu_run_test(check_console("1 +", "",					CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("+", "",						CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 +", "",					CONSOLE_RC_OK,				1, (console_int_t)3));
	mu_run_test(check_console("1 -", "",					CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("-", "",						CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 -", "",					CONSOLE_RC_OK,				1, (console_int_t)-1));
	mu_run_test(check_console("NEGATE", "",					CONSOLE_RC_ERR_DSTACK_UNDERFLOW,	(console_int_t)0));
	mu_run_test(check_console("1 NEGATE", "",				CONSOLE_RC_OK,				1, (console_int_t)-1));
	
	// Comments
	mu_run_test(check_console("1 2 # 3 4", "",				CONSOLE_RC_OK,				2, (console_int_t)1, (console_int_t)2));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 6)
	// Test Accept
	mu_run_test(check_accept(0,								0,								CONSOLE_RC_OK));
	mu_run_test(check_accept(1,								1,								CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_OK));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+1,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_ERR_ACCEPT_BUFFER_OVERFLOW));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+2,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_RC_ERR_ACCEPT_BUFFER_OVERFLOW));
#endif

	Serial.println(mk_msg(PSTR("Tests run: %d, failed: %d."), tests_run, tests_fail));
	while (1) ;
}
