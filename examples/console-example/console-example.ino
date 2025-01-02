#include <Arduino.h>

#include "console.h"
#include "console-internals.h"
#include "FConsole.h"

// Needed for user handler.

static bool console_cmds_user(char* cmd) {
	switch (console_hash(cmd)) {
		case /** + **/ 0XB58E: console_binop(+); break;
		case /** - **/ 0XB588: console_binop(-); break;
		case /** NEGATE **/ 0X7A79: console_unop(-); break;
		case /** # **/ 0XB586: console_raise(CONSOLE_RC_STATUS_IGNORE_TO_EOL); break;
		case /** LED **/ 0XDC88: digitalWrite(LED_BUILTIN, !!console_u_pop()); break;
		default: return false;
	}
	return true;
}

void setup() {
	Serial.begin(115200);
	pinMode(LED_BUILTIN, OUTPUT);										// We will drive the LED.
	
	FConsole.begin(console_cmds_user, Serial);

	// Signon message, note two newlines to leave a gap from any preceding output on the terminal.
	// Also note no following newline as the console prints one at startup, then a prompt. 
	consolePrint(CONSOLE_PRINT_STR_P, (console_cell_t)PSTR(CONSOLE_OUTPUT_NEWLINE_STR CONSOLE_OUTPUT_NEWLINE_STR "Arduino Console Example"));
	
	FConsole.prompt();
}

void loop() {
	FConsole.service();
}
