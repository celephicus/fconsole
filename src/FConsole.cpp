// Arduino console that is easy to use like all good Arduino libraries.
#include <Arduino.h>

#include "console.h"
#include "FConsole.h"
#include "console-internals.h"

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

// Single instance of Arduino console. Poor man's Singleton, don't create more than one. 
_FConsole FConsole;

/* We have an Arduino print function that requires a Stream instance to print on. This is held in FConsole. 
	for testing you can set this in FConsole and not use any of its other functions. */
void consolePrint(uint8_t opt, console_cell_t x) {
	if (FConsole.s_stream) {			// If an output stream has not been set do nothing. 
		switch (opt & 0x7f) {
			case CONSOLE_PRINT_NEWLINE:		FConsole.s_stream->print(F(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; return; 	// No separator.
			default:						(void)x; return;															// Ignore, print nothing. 
			case CONSOLE_PRINT_SIGNED:		FConsole.s_stream->print(x, DEC); break;
			case CONSOLE_PRINT_UNSIGNED:	FConsole.s_stream->print('+'); FConsole.s_stream->print((console_ucell_t)x, DEC); break;
			case CONSOLE_PRINT_HEX:			FConsole.s_stream->print('$'); 
											for (console_ucell_t m = 0xf; CONSOLE_UCELL_MAX != m; m = (m << 4) | 0xf) {
												if ((console_ucell_t)x <= m) 
													FConsole.s_stream->print(0);
											} 
											FConsole.s_stream->print((console_ucell_t)x, HEX); break;
			case CONSOLE_PRINT_STR:			FConsole.s_stream->print((const char*)x); break;
			case CONSOLE_PRINT_STR_P:		FConsole.s_stream->print((const __FlashStringHelper*)x); break;
			case CONSOLE_PRINT_CHAR:		FConsole.s_stream->print((char)x); break;
		}
		if (!(opt & CONSOLE_PRINT_NO_SEP))	FConsole.s_stream->print(' ');			// Print a space.
	}
}

static void print_console_seperator() { consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("->")); }

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
	consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(CONSOLE_OUTPUT_NEWLINE_STR ">"));
}

void _FConsole::service() {
	if (FConsole.s_stream && (FConsole.s_stream->available())) {			// If a character is available...
		const char c = FConsole.s_stream->read();
		console_rc_t rc = consoleAccept(c);						// Add it to the input buffer.
		if (rc >= CONSOLE_RC_OK) {								// On newline...
			consolePrint(CONSOLE_PRINT_STR, (console_cell_t)consoleAcceptBuffer());	// Echo input line back to terminal. 
			print_console_seperator();							// Seperator string for output. 
			if (CONSOLE_RC_OK == rc)							// If accept has _NOT_ returned an error process the input...	 
				rc = consoleProcess(consoleAcceptBuffer());		// Process input string from input buffer filled by accept and record error code. 
			if (CONSOLE_RC_OK != rc) {							// If all went well then we get an OK status code
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("Error:")); // Print error code:(
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)consoleGetErrorDescription(rc)); // Print description.
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(":"));
				consolePrint(CONSOLE_PRINT_SIGNED, (console_cell_t)rc);
			}
			prompt();								// In any case print a newline and prompt ready for the next line of input. 
		}
	}
}
