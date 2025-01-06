#include "console-config.sample.h"

// Very small for testing.
#undef CONSOLE_DATA_STACK_SIZE
#define CONSOLE_DATA_STACK_SIZE (4)

// Override printf used for output.
#undef CONSOLE_PRINTF
void console_printf(const char*fmt, ...);
#define CONSOLE_PRINTF console_printf

// We want example commands for trying out functionality.
#define CONSOLE_WANT_EXAMPLE_COMMANDS
