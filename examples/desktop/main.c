#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "console.h"

/* The number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
	can have potentially any hash value so could look like commands. */
static const console_recogniser_func RECOGNISERS[] = {
	console_r_number_decimal,
	console_r_number_hex,
	console_r_string,
	console_r_hex_string,
	console_cmds_builtin,
#ifdef CONSOLE_WANT_HELP
	console_cmds_help, 
#endif // CONSOLE_WANT_HELP
	console_cmds_example,
	NULL
};

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
	consoleInit(RECOGNISERS);							// Setup console.
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

void consolePrint(console_small_uint_t opt, console_int_t x) { consolePrintStream(stdout, opt, x); }
