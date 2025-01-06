#include "console-config.sample.h"

#undef CONSOLE_INPUT_NEWLINE_CHAR
#define CONSOLE_INPUT_NEWLINE_CHAR '\n'

// Need for printf()
#include <stdio.h>

// We want example commands for trying out functionality.
#define CONSOLE_WANT_EXAMPLE_COMMANDS

// And a sample user command.
bool console_cmds_user(char* cmd);
#undef CONSOLE_USER_RECOGNISERS
#define CONSOLE_USER_RECOGNISERS console_cmds_user,
