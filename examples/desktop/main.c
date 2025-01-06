#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "console.h"

bool console_cmds_user(char* cmd) {
	switch (console_hash(cmd)) {
		case /** 2+ (x1 - x2) Add 2 to TOS. **/ 0x685c: console_u_tos() += 2; break;
		default: return false;
	}
	return true;
}

static void prompt(void) { fputs("\n> ", stdout); }
static void seperator(void) { fputs(" -> ", stdout); }

// Linux requires this to emulate TurboC getch(). Copied from Stackoverflow
#include <termios.h>
#include <unistd.h>

/* reads from keypress, doesn't echo */
static int getch(void) {
    struct termios oldattr, newattr;
    int ch;
    tcgetattr( STDIN_FILENO, &oldattr );
    newattr = oldattr;
    newattr.c_lflag &= (tcflag_t)~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newattr );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldattr );
    return ch;
}

int main(int argc, char **argv) {
	(void)argc; (void)argv;
	consoleInit();							// Setup console.
	printf("\n\n" 
	      "FConsole Example -- `exit' to quit.");
	prompt();

	while (1) {
		const char c = (char)getch();
		if (CONSOLE_INPUT_NEWLINE_CHAR != c)			// Don't echo newline.
			putchar(c);
		console_rc_t rc = consoleAccept(c);				// Add it to the input buffer.
		if (rc >= CONSOLE_RC_OK) {						// On newline...
			const char* cmd = "??";						// Last command on error.
			seperator(); 								// Seperator string for output.
			if (CONSOLE_RC_OK == rc)					// Only process if no error from accept...
				rc = consoleProcess(consoleAcceptBuffer(), &cmd);	// Process input and record new error code.
			if (CONSOLE_RC_OK != rc) {					// If all went well then we get an OK status code.
				if (CONSOLE_RC_ERR_USER == rc) {		// Exit error code.
					puts("Bye...");
					break;
				}
				printf("Error in command `%s': %s (%d)", cmd , consoleGetErrorDescription(rc), rc);
			}
			prompt();							// In any case print a newline and prompt ready for the next line of input.
		}
	}
	return 0;
}
