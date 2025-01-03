// Arduino console that is easy to use like all good Arduino libraries.
#include <Arduino.h>

#include "console.h"
#include "fconsole.h"
#include "console-internals.h"

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

// Single instance of Arduino console. Poor man's Singleton, don't create more than one.
_FConsole FConsole;

/* We have an Arduino print function that requires a Stream instance to print on. This is held in FConsole.
	for testing you can set this in FConsole and not use any of its other functions. */
void consolePrint(uint_least8_t opt, console_int_t x) {
	if (FConsole.s_stream) {			// If an output stream has not been set do nothing.
		switch (opt & ~(CONSOLE_PRINT_NO_SEP|CONSOLE_PRINT_NO_LEAD)) {
			case CONSOLE_PRINT_NEWLINE:		FConsole.s_stream->print(F(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; return; 	// No separator.
			default:						(void)x; return;															// Ignore, print nothing.
			case CONSOLE_PRINT_SIGNED:		FConsole.s_stream->print(x, DEC); break;
			case CONSOLE_PRINT_UNSIGNED:	if (opt & CONSOLE_PRINT_NO_LEAD) FConsole.s_stream->print('+'); 
											FConsole.s_stream->print((console_uint_t)x, DEC); break;
			case CONSOLE_PRINT_HEX2:		if (opt & CONSOLE_PRINT_NO_LEAD) FConsole.s_stream->print('$');
											if ((console_uint_t)x <= 0x0f) FConsole.s_stream->print(0);
											FConsole.s_stream->print((console_uint_t)x & 0xffU, HEX); break;
			case CONSOLE_PRINT_HEX:			if (opt & CONSOLE_PRINT_NO_LEAD) FConsole.s_stream->print('$');
											for (console_uint_t m = 0xf; CONSOLE_UINT_MAX != m; m = (m << 4) | 0xf) {
												if ((console_uint_t)x <= m)
													FConsole.s_stream->print(0);
											}
											FConsole.s_stream->print((console_uint_t)x, HEX); break;
			case CONSOLE_PRINT_STR:			FConsole.s_stream->print((const char*)x); break;
			case CONSOLE_PRINT_STR_P:		FConsole.s_stream->print((const __FlashStringHelper*)x); break;
			case CONSOLE_PRINT_CHAR:		FConsole.s_stream->print((char)x); break;
		}
		if (!(opt & CONSOLE_PRINT_NO_SEP))	FConsole.s_stream->print(' ');			// Print a space.
	}
}

static void print_console_seperator() { consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)PSTR("->")); }

/* The number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
	can have potentially any hash value so could look like commands. */
static const console_recogniser_func RECOGNISERS[] PROGMEM = {
	console_r_number_decimal,
	console_r_number_hex,
	console_r_string,
	console_r_hex_string,
	console_cmds_builtin,
	_FConsole::r_cmds_user,			// Static member function, so it does not need a `this' pointer.
	NULL
};

// Static members.
console_recogniser_func _FConsole::s_r_user;
Stream* _FConsole::s_stream;

void _FConsole::begin(console_recogniser_func r_user, Stream& s) {
	s_r_user = r_user;									// Set user recogniser function.
	s_stream = &s;										// Set stream for IO.
	consoleInit(RECOGNISERS);							// Setup console.
}
void _FConsole::prompt() {
	consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)PSTR(CONSOLE_OUTPUT_NEWLINE_STR ">"));
}

void _FConsole::service() {
	if (FConsole.s_stream && (FConsole.s_stream->available())) {			// If a character is available...
		const char c = FConsole.s_stream->read();
		console_rc_t rc = consoleAccept(c);						// Add it to the input buffer.
		if (rc >= CONSOLE_RC_OK) {								// On newline...
			consolePrint(CONSOLE_PRINT_STR, (console_int_t)consoleAcceptBuffer());	// Echo input line back to terminal.
			print_console_seperator();							// Seperator string for output.
			if (CONSOLE_RC_OK == rc)							// If accept has _NOT_ returned an error process the input...
				rc = consoleProcess(consoleAcceptBuffer());		// Process input string from input buffer filled by accept and record error code.
			if (CONSOLE_RC_OK != rc) {							// If all went well then we get an OK status code
				consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)PSTR("Error:")); // Print error code:(
				consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)consoleGetErrorDescription(rc)); // Print description.
				consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)PSTR(":"));
				consolePrint(CONSOLE_PRINT_SIGNED, (console_int_t)rc);
			}
			prompt();								// In any case print a newline and prompt ready for the next line of input.
		}
	}
}
