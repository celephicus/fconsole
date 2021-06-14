#include <Arduino.h>

#include "console.h"

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

// Copied from the StringStream Arduino library.
class StringStream : public Stream
{
public:
    StringStream(String &s) : string(s), position(0) { }

    // Stream methods
    virtual int available() { return string.length() - position; }
    virtual int read() { return position < string.length() ? string[position++] : -1; }
    virtual int peek() { return position < string.length() ? string[position] : -1; }
    virtual void flush() { };
    // Print methods
    virtual size_t write(uint8_t c) { string += (char)c; return 1;};

private:
    String &string;
    unsigned int length;
    unsigned int position;
};

// Our console outputs to a string now.
String o_str;
StringStream o_stream(o_str);

void setup() {
	Serial.begin(115200);
	Serial.println();
	Serial.println(F("Arduino Console Unit Tests"));
	consoleInit(&o_stream);
}

// Test code from adapted from minunit: http://www.jera.com/techinfo/jtns/jtn002.html
// It is a real mess, uses far too much memory, constant strings are in RAM rather than Progmem, etc.
static char msg[100];
char* mk_msg(const char* fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsprintf(msg, fmt, ap);
	return msg;
}

int tests_run, tests_fail;
#define mkstr(s_) #s_
#define mu_init() { tests_run = tests_fail = 0; }
#define mu_run_test(_test) do { tests_run++; char* msg = _test; if (msg) { Serial.print("Fail: " mkstr(_test) ": "); Serial.println(msg); } } while (0)

#define mu_assert_equal(val_, expected_) mu_assert(mkstr(val_) "!=" mkstr(expected_), (val_) != (expected_))

#define mu_assert_equal_str(val_, expected_) if (0 != strcmp((val_), (expected_))) { tests_fail++; return mk_msg("%s != %s; expected `%s', got `%s'", mkstr(val_), mkstr(expected_), expected_, val_); }
#define mu_assert_equal_int(val_, expected_) if ((val_) != (expected_))            { tests_fail++; return mk_msg("%s != %s; expected `%d', got `%d'", mkstr(val_), mkstr(expected_), expected_, val_); }

char* check_console(const char* input, const char* output, uint8_t rc_expected, int8_t depth_expected, ...) {
	char inbuf[32];								// Copy input string as we are not measnt to write to string literals.
	strcpy(inbuf, input);
	o_str = "";									// Clear output string of contents from previous test.. 
	consoleReset();								// Clear stack. 
	uint8_t rc = consoleProcess(inbuf);			// Process input string.
	mu_assert_equal_int(rc, rc_expected);		// Verify return code...
	mu_assert_equal_str(output, o_str.c_str());	// Verify output string...
	
	// Verify stack, starting with depth, then contents.
	mu_assert_equal_int(consoleStackDepth(), depth_expected); 
	va_list ap;
	va_start(ap, depth_expected);
	while (depth_expected-- > 0) {
		if (consoleStackPick(depth_expected) != va_arg(ap, console_cell_t)) { 
			sprintf(msg, " stack: "); 
			for (uint8_t i = 0; i < consoleStackDepth(); i += 1) 
				sprintf(msg+strlen(msg), "%d ", consoleStackPick(i)); 
			return msg;
		}
	}
	return NULL;
}

char* check_accept(uint8_t char_count, uint8_t len_expected, uint8_t rc_expected) {
	consoleAcceptClear();
	uint8_t rc;
	
	// Checks will accept up to CONSOLE_INPUT_BUFFER_SIZE chars.
	char chk[100];
	char* cp = chk;
	for (uint8_t i = 0; i < char_count; i += 1) {
		rc = consoleAccept(*cp++ = ('!' + i));				// Call accept with a printable character.
		mu_assert_equal_int(CONSOLE_ACCEPT_PENDING, rc);
	}
	*cp = '\0';

	rc = consoleAccept(CONSOLE_INPUT_NEWLINE_CHAR);
	mu_assert_equal_int(rc_expected, rc);
	if (CONSOLE_ACCEPT_DONE == rc) {
		mu_assert_equal_int(strlen(consoleAcceptBuffer()), len_expected);				// Verify expected length of string in buffer
		mu_assert_equal_int(strncmp(consoleAcceptBuffer(), chk, len_expected), 0);		// Verify contents with data that was sent to consoleAccept().
	}
	return NULL;
}

// Since I had a Pro Mini handy all tests won't fit in the memory, so they have to be tested in batches.
#define TEST_BATCH 1 
void loop() {
	mu_init();
#if (TEST_BATCH == 0) || (TEST_BATCH == 1)
	// Command parsing
	mu_run_test(check_console("", "",						CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console(" ", "",						CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("\t", "",						CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("foo", "",					CONSOLE_ABORT_UNKNOWN_COMMAND,	0));
	
	// Check decimal number parser.
//	mu_run_test(check_console("+", "",						CONSOLE_ABORT_UNKNOWN_COMMAND,	0));
//	mu_run_test(check_console("-", "",						CONSOLE_ABORT_UNKNOWN_COMMAND,	0));
	mu_run_test(check_console("0", "",						CONSOLE_ABORT_OK,				1, 0));
	mu_run_test(check_console("+0", "",						CONSOLE_ABORT_OK,				1, 0));
	mu_run_test(check_console("1", "",						CONSOLE_ABORT_OK,				1, 1));
	mu_run_test(check_console("32767", "",					CONSOLE_ABORT_OK,				1, 32767));
	mu_run_test(check_console("-32768", "",					CONSOLE_ABORT_OK,				1, -32768));
	mu_run_test(check_console("32768", "",					CONSOLE_ABORT_NUMBER_OVERFLOW,	0));
	mu_run_test(check_console("-32769", "",					CONSOLE_ABORT_NUMBER_OVERFLOW,	0));
	mu_run_test(check_console("+32768", "",					CONSOLE_ABORT_OK,				1, 32768));
	mu_run_test(check_console("+65535", "",					CONSOLE_ABORT_OK,				1, 65535));
	mu_run_test(check_console("+65536", "",					CONSOLE_ABORT_NUMBER_OVERFLOW,	0));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 2)
	// Check hex number parser.
	mu_run_test(check_console("$", "",						CONSOLE_ABORT_UNKNOWN_COMMAND,	0));
	mu_run_test(check_console("$1", "",						CONSOLE_ABORT_OK,				1, 1));
	mu_run_test(check_console("$FFFF", "",					CONSOLE_ABORT_OK,				1, 0xffff));
	mu_run_test(check_console("$10000", "",					CONSOLE_ABORT_NUMBER_OVERFLOW,	0));
	
	// Check string parser & string printer.
	mu_run_test(check_console(".\"", "",					CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("\" .\"", " ",				CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("\"\\\\ .\"", "\\ ",			CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("\"q .\"", "q ",				CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("\"q\\n\\r\\ .\"", "q\n\r  ",	CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("\"\\1f .\"", "\x1f ",		CONSOLE_ABORT_OK,				0));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 3)
	// Number printing.
	mu_run_test(check_console(".", "",						CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("0 .", "0 ",					CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("32767 .", "32767 ",			CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("-32768 .", "-32768 ",		CONSOLE_ABORT_OK,				0));

	mu_run_test(check_console("U.", "",						CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("0 U.", "+0 ",				CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("+65535 U.", "+65535 ",		CONSOLE_ABORT_OK,				0));
		
	mu_run_test(check_console("$.", "",						CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("$FFFF $.", "$FFFF ",			CONSOLE_ABORT_OK,				0));
	mu_run_test(check_console("$AF19 $.", "$AF19 ",			CONSOLE_ABORT_OK,				0));

	// Stack over/under flow.
	mu_run_test(check_console("DROP", "",					CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 3 4 5 6 7 8 9 ", "",		CONSOLE_ABORT_DSTACK_OVERFLOW,	8, 1, 2, 3, 4, 5, 6, 7, 8));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 4)
	// Commands
	mu_run_test(check_console("1 2 DROP", "",				CONSOLE_ABORT_OK,				1, 1));
	mu_run_test(check_console("1 2 drop", "",				CONSOLE_ABORT_OK,				1, 1));
	mu_run_test(check_console("\"HASH HASH", "",			CONSOLE_ABORT_OK,				1, 0x90b7));
	mu_run_test(check_console("\"hash HASH", "",			CONSOLE_ABORT_OK,				1, 0x90b7));
	mu_run_test(check_console("1 2 CLEAR", "",				CONSOLE_ABORT_OK,				0));

	// User commands.
	mu_run_test(check_console("1 +", "",					CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("+", "",						CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 +", "",					CONSOLE_ABORT_OK,				1, 3));
	mu_run_test(check_console("1 -", "",					CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("-", "",						CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 2 -", "",					CONSOLE_ABORT_OK,				1, -1));
	mu_run_test(check_console("NEGATE", "",					CONSOLE_ABORT_DSTACK_UNDERFLOW,	0));
	mu_run_test(check_console("1 NEGATE", "",				CONSOLE_ABORT_OK,				1, -1));
	
	// Comments
	mu_run_test(check_console("1 2 # 3 4", "",				CONSOLE_ABORT_OK,				2, 1, 2));
#endif
#if (TEST_BATCH == 0) || (TEST_BATCH == 5)
	// Test Accept
	mu_run_test(check_accept(0,								0,								CONSOLE_ACCEPT_DONE));
	mu_run_test(check_accept(1,								1,								CONSOLE_ACCEPT_DONE));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_INPUT_BUFFER_SIZE-1,	CONSOLE_ACCEPT_DONE));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_ACCEPT_DONE));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+1,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_ACCEPT_OVERFLOW));
	mu_run_test(check_accept(CONSOLE_INPUT_BUFFER_SIZE+2,	CONSOLE_INPUT_BUFFER_SIZE,		CONSOLE_ACCEPT_OVERFLOW));
#endif

	Serial.println(mk_msg("Tests run: %d, failed: %d.", tests_run, tests_fail));
	while (1) ;
}
