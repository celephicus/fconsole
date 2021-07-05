#include <Arduino.h>

#include "FConsole.h"

void setup() {
	Serial.begin(115200);
	pinMode(LED_BUILTIN, OUTPUT);										// We will drive the LED.
	FConsole.begin(Serial);
}

void loop() {
	FConsole.service();
}
