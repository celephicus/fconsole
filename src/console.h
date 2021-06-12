#ifndef CONSOLE_H__
#define CONSOLE_H__

// Our little language only works with 16 bit words, called "cells" like FORTH. 
typedef int16_t console_cell_t;
typedef uint16_t console_ucell_t;
#define CONSOLE_UCELL_MAX ((console_ucell_t)0xffff)
#define CONSOLE_CELL_MAX ((console_cell_t)0x7fff)
#define CONSOLE_CELL_MIN ((console_cell_t)0x8000)

// Stack size, we don't need much.
#define CONSOLE_DATA_STACK_SIZE (8)

#define CONSOLE_INPUT_BUFFER_SIZE 4
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

// Initialise with a stream for IO. 
void consoleInit(Stream* output_stream);

// Possible abort codes from eval().
enum { 
	CONSOLE_ABORT_OK,
	CONSOLE_ABORT_NUMBER_OVERFLOW,
	CONSOLE_ABORT_INTERNAL_ERROR,			// Not used at present...
	CONSOLE_ABORT_DSTACK_UNDERFLOW,			// Attempt to pop on an empty stack.
	CONSOLE_ABORT_DSTACK_OVERFLOW,			// Attempt to push on an empty stack.
	CONSOLE_ABORT_IGNORE_TO_EOL,			// Used to implement comments.
	CONSOLE_ABORT_UNKNOWN_COMMAND,			// Constant or command not recognised.
};
		
// Evaluate a line of string input.
uint8_t consoleProcess(char* str);

// Input functions, not essential but helpful.
void consoleAcceptClear();
enum {
	CONSOLE_ACCEPT_DONE,
	CONSOLE_ACCEPT_OVERFLOW,
	CONSOLE_ACCEPT_PENDING,
};
uint8_t consoleAccept(char c);
char* consoleAcceptBuffer();

// These functions are for unit tests and should not be used for real code.
uint8_t consoleStackDepth();
console_cell_t consoleStackPick(uint8_t i);
void consoleReset();
uint8_t consoleAcceptBufferLen();

#endif // CONSOLE_H__

