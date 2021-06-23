#include <Arduino.h>

#include "console.h"

#include <Stream.h>
#include <stdarg.h>
#include <stdio.h>

#define CONSOLE_OUTPUT_STREAM Serial
void consolePrint(uint8_t s, console_cell_t x) {
	switch (s) {
		case CONSOLE_PRINT_NEWLINE:		CONSOLE_OUTPUT_STREAM.print(F("\r\n")); (void)x; break;
		case CONSOLE_PRINT_SIGNED:		CONSOLE_OUTPUT_STREAM.print(x, DEC); CONSOLE_OUTPUT_STREAM.print(' '); break;
		case CONSOLE_PRINT_UNSIGNED:	CONSOLE_OUTPUT_STREAM.print('+'); CONSOLE_OUTPUT_STREAM.print((console_ucell_t)x, DEC); CONSOLE_OUTPUT_STREAM.print(' '); break;
		case CONSOLE_PRINT_HEX:			CONSOLE_OUTPUT_STREAM.print('$'); {
											console_ucell_t m = 0xf;
											do {
												if ((console_ucell_t)x <= m) CONSOLE_OUTPUT_STREAM.print(0);
												m = (m << 4) | 0xf;
											} while (CONSOLE_UCELL_MAX != m);
										} CONSOLE_OUTPUT_STREAM.print((console_ucell_t)x, HEX); CONSOLE_OUTPUT_STREAM.print(' '); break;
		case CONSOLE_PRINT_STR:			CONSOLE_OUTPUT_STREAM.print((const char*)x); CONSOLE_OUTPUT_STREAM.print(' '); break;
		case CONSOLE_PRINT_STR_P:		CONSOLE_OUTPUT_STREAM.print((const __FlashStringHelper*)x); CONSOLE_OUTPUT_STREAM.print(' '); break;
		default:						/* ignore */; break;
	}
}

static void print_console_prompt() { consolePrint(CONSOLE_PRINT_NEWLINE, 0); consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(">"));}
static void print_console_seperator() {	 consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(" -> ")); }

void setup() {
	Serial.begin(115200);
	pinMode(13, OUTPUT);										// We will drive the LED.
	
	// Signon message.
	consolePrint(CONSOLE_PRINT_NEWLINE, 0); 
	consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("Arduino Console Example"));
	
	consoleInit();												// Setup console.
	print_console_prompt();										// Start off with a prompt.
}

void loop() {
	if (Serial.available()) {									// If a character is available...
		const char c = Serial.read();
		console_rc_t rc = consoleAccept(c);						// Add it to the input buffer.
		if (CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW == rc) {		// Check for overflow...
			consolePrint(CONSOLE_PRINT_STR, (console_cell_t)consoleAcceptBuffer());				// Echo input line back to terminal. 
			print_console_seperator();		
			consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("Input buffer overflow."));
			print_console_prompt();
		}
		else if (CONSOLE_RC_OK == rc) {						// Check for a newline...
			consolePrint(CONSOLE_PRINT_STR, (console_cell_t)consoleAcceptBuffer());				// Echo input line back to terminal. 
			print_console_seperator();							// Print separator before output.
			rc = consoleProcess(consoleAcceptBuffer());			// Process input string from input buffer filled by accept and record error code. 
			if (CONSOLE_RC_OK != rc) {						// If all went well then we get an OK status code
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR("Error: "));						// Print error code:(
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)consoleGetErrorDescription(rc));	// Print description.
				consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(": "));
				consolePrint(CONSOLE_PRINT_SIGNED, (console_cell_t)PSTR(rc);
			}
			print_console_prompt();								// In any case print a newline and prompt ready for the next line of input. 
		}
	}
}
