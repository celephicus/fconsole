#include <Arduino.h>

#include "FConsole.h"

// Needed for user handler.
#include "console.h"
#include "console-internals.h"
bool console_cmds_user(char* cmd) {
	switch (console_hash(cmd)) {
		case /** + **/ 0xb58e: console_binop(+); break;
		case /** - **/ 0xb588: console_binop(-); break;
		case /** NEGATE **/ 0x7a79: console_unop(-); break;
		case /** # **/ 0xb586: console_raise(CONSOLE_RC_SIGNAL_IGNORE_TO_EOL); break;
		case /** LED **/ 0xdc88: digitalWrite(LED_BUILTIN, !!console_u_pop()); break;
		default: return false;
	}
	return true;
}

void setup() {
	Serial.begin(115200);
	pinMode(LED_BUILTIN, OUTPUT);										// We will drive the LED.
	FConsole.begin(Serial);
}

void loop() {
	FConsole.service();
}
