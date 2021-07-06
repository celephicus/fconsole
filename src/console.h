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

// Input buffer 
#define CONSOLE_INPUT_BUFFER_SIZE 40
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

/* Recognisers are little parser functions that can turn a string into a value or values that are pushed onto the stack. They return 
	false if they cannot parse the input string. If they do parse it, they might call raise() if they cannot push a value onto the stack. */
typedef bool (*console_recogniser_func)(char* cmd);

/* Initialise the console with the NULL-terminated list of recogniser functions (in PROGMEM) that are tried in order. 
	If one returns false, then the next recogniser is called. If a recogniser returns true then the command is taken to have worked. If 
	a recogniser calls console_raise() then the process aborts with the error code.  */
void consoleInit(const console_recogniser_func* r_list);

// Function to print on the output stream. You must supply this. An example is in a comment in console.cpp. Unknown options are ignored.
#define CONSOLE_OUTPUT_NEWLINE_STR "\r\n"
enum {
	CONSOLE_PRINT_NEWLINE,		// Prints the newline string CONSOLE_OUTPUT_NEWLINE_STR, second arg ignored. 
	CONSOLE_PRINT_SIGNED,		// Prints second arg as a signed integer, e.g `-123 ', `0 ', `456 ', note trailing SPACE.
	CONSOLE_PRINT_UNSIGNED,		// Print second arg as an unsigned integer, e.g `+0 ', `+123 ', note trailing SPACE.
	CONSOLE_PRINT_HEX,			// Print second arg as a hex integer, e.g `$0000 ', `$abcd ', note trailing SPACE.
	CONSOLE_PRINT_STR,			// Print second arg as pointer to string in RAM, with trailing space. 
	CONSOLE_PRINT_STR_P,		// Print second arg as pointer to string in PROGMEM, with trailing space. 
};
void consolePrint(uint8_t opt, console_cell_t x);

// User recogniser hook, define your own commands here.
//bool console_cmds_user(char* cmd);

// Define possible error codes. The convention is that positive codes are actual errors, zero is OK, and negative values are more like status codes that do not indicate an error.
enum {
	CONSOLE_RC_OK =								0,	// Returned by consoleProcess() for no errors and by consoleAccept() if a newline was read with no overflow.
	
	// Errors: something has gone wrong.
	CONSOLE_RC_ERROR_NUMBER_OVERFLOW =			1,	// Returned by consoleProcess() (via convert_number()) if a number overflowed it's allowed bounds,
	CONSOLE_RC_ERROR_DSTACK_UNDERFLOW =			2,	// Stack underflowed (attempt to pop or examine too many items).
	CONSOLE_RC_ERROR_DSTACK_OVERFLOW =			3,	// Stack overflowed (attempt to push too many items).
	CONSOLE_RC_ERROR_UNKNOWN_COMMAND =			4,	// A command or value was not recognised.
	CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW =	5,	// Accept input buffer has been sent more characters than it can hold. Only returned by consoleAccept(). 
	CONSOLE_RC_ERROR_INDEX_OUT_OF_RANGE =		6,	// Index out of range.
	CONSOLE_RC_ERROR_USER,							// Error codes available for the user.
	
	// Status
	CONSOLE_RC_STATUS_IGNORE_TO_EOL =			-1,	// Internal signal used to implement comments.
	CONSOLE_RC_STATUS_ACCEPT_PENDING =			-2,	// Only returned by consoleAccept() to signal that it is still accepting characters.
	CONSOLE_RC_STATUS_USER =					-3	// Status codes available for the user.
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

// Return depth of stack, useful for testing.
uint8_t consoleStackDepth();		

// Return stack values from the top down, if you go beyond (depth-1) you might read outside valid memory.
console_cell_t consoleStackPick(uint8_t i);

/* Resets the state of the console to what it would be after initialisation. Note does not affect the state of accept. 
	Useful for testing. */
void consoleReset();

#endif // CONSOLE_H__

