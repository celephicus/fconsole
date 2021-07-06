// Arduino console that is easy to use like all good Arduino libraries.
#include <Arduino.h>

#include "console.h"
#include "FConsole.h"

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

_FConsole FConsole;

void consolePrint(uint8_t s, console_cell_t x) {
	if (FConsole._s) {
		switch (s) {
			case CONSOLE_PRINT_NEWLINE:		FConsole._s->print(F(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; return; // No trailing space.
			default:						/* ignore */; return;												// No trailing space.
			case CONSOLE_PRINT_SIGNED:		FConsole._s->print(x, DEC); break;
			case CONSOLE_PRINT_UNSIGNED:	FConsole._s->print('+'); FConsole._s->print((console_ucell_t)x, DEC); break;
			case CONSOLE_PRINT_HEX:			FConsole._s->print('$'); 
											for (console_ucell_t m = 0xf; CONSOLE_UCELL_MAX != m; m = (m << 4) | 0xf) {
												if ((console_ucell_t)x <= m) 
													FConsole._s->print(0);
											} 
											FConsole._s->print((console_ucell_t)x, HEX); break;
			case CONSOLE_PRINT_STR:			FConsole._s->print((const char*)x); break;
			case CONSOLE_PRINT_STR_P:		FConsole._s->print((const __FlashStringHelper*)x); break;
		}
		FConsole._s->print(' '); 			// Print trailing space here. 
	}
}

static void print_console_prompt() { consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(CONSOLE_OUTPUT_NEWLINE_STR ">"));}
static void print_console_seperator() {	consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("->")); }

void _FConsole::begin(Stream& s) {
	_s = &s;													// Set stream for IO. 
	consoleInit();												// Setup console.
	print_console_prompt();										// Start off with a prompt.
}

void _FConsole::service() {
	if (FConsole._s && (FConsole._s->available())) {			// If a character is available...
		const char c = FConsole._s->read();
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
			print_console_prompt();								// In any case print a newline and prompt ready for the next line of input. 
		}
	}
}
