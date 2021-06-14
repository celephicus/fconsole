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

#define CONSOLE_INPUT_BUFFER_SIZE 40
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

// Initialise with a stream for IO. 
void consoleInit(Stream* output_stream);

// Define possible error codes and a corresponding error message with the magic of x-macros.
// Why do it like this? So that the messages can _never_ get out of step with the codes. and it allows the addition of more codes & messages.
// And it's a powerful technique that is worth learning. See https://en.wikipedia.org/wiki/X_Macro.
#define CONSOLE_ERROR_DEFS(X)																	\
 X(OK,						"success")															\
 X(NUMBER_OVERFLOW,			"number overflow")													\
 X(DSTACK_UNDERFLOW,		"data stack underflow")												\
 X(DSTACK_OVERFLOW,			"data stack overflow")												\
 X(UNKNOWN_COMMAND,			"unknown command")													\
 X(INPUT_BUFFER_OVERFLOW,	"input buffer overflow")	/* Only returned by consoleAccept(). */	\
 X(IGNORE_TO_EOL,			"")							/* Never seen by caller. */				\
 X(ACCEPT_PENDING,			"")							/* Only returned by consoleAccept(). */	\
 
// Macro that generates error enum items.
#define X_CONSOLE_ERROR_CODES_ENUM(name_, desc_) 		\
 CONSOLE_ERROR_##name_,

// Macros that generate a string description
#define X_CONSOLE_ERROR_CODES_DESCRIPTION_STR_DEF(name_, desc_) 				\
    static const char CONSOLE_ERROR_CODE_DESCR_##name_[] PROGMEM = desc_;
#define X_CONSOLE_ERROR_CODES_DESCRIPTION_STR(name_, desc_) 					\
    CONSOLE_ERROR_CODE_DESCR_##name_,

// Declare the error values. These are only assigned to a console_rc_t, this is signed as we might want negative codes sometime.
enum {
	CONSOLE_ERROR_DEFS(X_CONSOLE_ERROR_CODES_ENUM)
	COUNT_CONSOLE_ERROR
};

typedef int8_t console_rc_t;

// Return a short description of the error as a pointer to a PROGMEM string. 
const char* consoleGetErrorDescription(console_rc_t err);

// Evaluate a line of string input.
console_rc_t consoleProcess(char* str);

// Input functions, not essential but helpful.
void consoleAcceptClear();

/* Read chars into a buffer, returning CONSOLE_ERROR_ACCEPT_PENDING. Only CONSOLE_INPUT_BUFFER_SIZE chars are stored.
	If the character CONSOLE_INPUT_NEWLINE_CHAR is seen, then return CONSOLE_ERROR_OK if no overflow, else CONSOLE_ERROR_INPUT_OVERFLOW.
	In either case the buffer is nul terminated, but not all chars will have been stored on overflow. 
*/
console_rc_t consoleAccept(char c);
char* consoleAcceptBuffer();

// These functions are for unit tests and should not be used for real code.
uint8_t consoleStackDepth();
console_cell_t consoleStackPick(uint8_t i);
void consoleReset();

#endif // CONSOLE_H__

