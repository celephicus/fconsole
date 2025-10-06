#ifndef CONSOLE_H__
#define CONSOLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "console-config.h"

/* Get max/min for types. This only works because we assume two's complement representation
 * and we have checked that the signed & unsigned types are compatible. */
#define CONSOLE_UINT_MAX (~(console_uint_t)(0))
#define CONSOLE_INT_MAX ((console_int_t)(CONSOLE_UINT_MAX >> 1))
#define CONSOLE_INT_MIN ((console_int_t)(~(CONSOLE_UINT_MAX >> 1)))

/* Recognisers are little parser functions that can turn a string into a value or values that are pushed onto the stack. They return
	false if they cannot parse the input string. If they do parse it, they might call raise() if they cannot push a value onto the stack. */
typedef bool (*console_recogniser_func)(char* cmd);

/* Initialise the console .  */
void consoleInit(void);

// Function to print on the output stream. You must supply this. An example is in a comment in console.cpp. Unknown options are ignored and cause no output.
enum {
	CONSOLE_PRINT_NEWLINE,		// Prints a newline, second arg ignored, no seperator is printed.
	CONSOLE_PRINT_SIGNED,		// Prints second arg as a signed integer, e.g `-123 ', `0 ', `456 ', note trailing SPACE.
	CONSOLE_PRINT_UNSIGNED,		// Print second arg as an unsigned integer, e.g `+0 ', `+123 ', note trailing SPACE.
	CONSOLE_PRINT_HEX,			// Print second arg as a hex integer, e.g `$0000 ', `$abcd ', note trailing SPACE.
	CONSOLE_PRINT_HEX2,			// Print 2 hex digits, e.g `$00 ', `$ab ', note trailing SPACE.
	CONSOLE_PRINT_STR,			// Print second arg as pointer to string in RAM, with trailing space.
	CONSOLE_PRINT_STR_P,		// Print second arg as pointer to string in PROGMEM, with trailing space.
	CONSOLE_PRINT_CHAR,			// Print second arg as char, with trailing space.
	CONSOLE_PRINT_NO_LEAD = 0x40,	// AND with option to _NOT_ print some leading characters.
	CONSOLE_PRINT_NO_SEP = 0x80	// AND with option to _NOT_ print a trailing space.
};
void consolePrint(console_small_uint_t opt, console_int_t x);

// Prototypes for various recogniser functions.

/* Recogniser for signed/unsigned decimal number. The number format is as follows:
	An initial '-' flags the number as negative, the '-' character is illegal anywhere else in the word.
	An initial '+' flags the number as unsigned. In which case the range of the number is up to the maximum _unsigned_ value.
	Abort codes: OVERFLOW if the value parses as a number but overflows the range. */
bool console_r_number_decimal(char* cmd);

// Recogniser for hex numbers preceded by a '$'.
bool console_r_number_hex(char* cmd);

// String with a leading '"' pushes address of string which is zero terminated.
bool console_r_string(char* cmd);

/* Hex string with a leading '&', then n pairs of hex digits, pushes address of length of string, then binary data.
	So `&1aff01' will push a pointer to memory 03 1a ff 01. */
bool console_r_hex_string(char* cmd);

// Essential commands that will always be required
bool console_cmds_builtin(char* cmd);

// Some "useful" commands used for testing and examples.
bool console_cmds_example(char* cmd);

// Optional help commands, will be empty if CONSOLE_WANT_HELP not defined.
bool console_cmds_help(char* cmd);

/* Define possible error codes. The convention is that positive codes are actual errors, zero is OK, and negative
	values are more like status codes that do not indicate an error.
	Errors are defined with an X macro as they have associated text. They will have codes increasing from 1. */
#define CONSOLE_DEF_ERROR_CODE(X) 																\
	X(NO_CHEESE, 	"++?????++ Out of Cheese Error. Redo From Start")	\
	X(NUM_OVF, 		"number overflow")																\
	X(DSTK_UNF, 	"stack underflow")																\
	X(DSTK_OVF, 	"stack overflow")																	\
	X(ACC_OVF, 		"input buffer overflow")													\
	X(BAD_IDX, 		"index out of range")															\
	X(BAD_CMD, 		"unknown command")																\
	X(DIV_ZERO, 	"divide by zero")

#define CONSOLE_DEF_ERROR_CODE_ENUM(v_, s_) CONSOLE_RC_ERR_ ## v_,
enum {
	CONSOLE_RC_OK =				0,	// Returned by consoleProcess() for no errors and by consoleAccept() for a newline with no overflow.

	// Errors: something has gone wrong...
	CONSOLE_DEF_ERROR_CODE(CONSOLE_DEF_ERROR_CODE_ENUM)

	CONSOLE_RC_ERR_USER,			// Error codes available for the user.

	// Status...
	CONSOLE_RC_STAT_IGN_EOL =		-1,		// Internal signal used to implement comments.
	CONSOLE_RC_STAT_ACC_PEND =	-2,		// Only returned by consoleAccept() to signal that it is still accepting characters.
	CONSOLE_RC_STAT_ACC_CAN = 	-3,		// Only returned by consoleAccept() to signal input cancelled.
	CONSOLE_RC_STAT_USER =			-4		// Status codes available for the user.
};
#undef CONSOLE_DEF_ERROR_CODE_ENUM

// Type for a console API call status code.
typedef console_small_int_t console_rc_t;

// Return a short description of the error as a pointer to a PROGMEM string.
const char* consoleGetErrorDescription(console_rc_t err);

/* Evaluate a line of string input. Note that the parser unusually writes back to the input string.
	It will never go beyond the terminating nul.
	If pointer current supplied it is set to command in the input buffer that has been executed. */
console_rc_t consoleProcess(char* str, const char** current);

// Input functions, may be helpful.

/* Resets the state of accept to what it was after calling consoleInit(), or after consoleAccept() has read a newline
 * and returned either CONSOLE_RC_OK or CONSOLE_ERROR_INPUT_OVERFLOW. */
void consoleAcceptClear(void);

/* Read chars into a buffer, returning CONSOLE_ERROR_ACCEPT_PENDING. Only CONSOLE_INPUT_BUFFER_SIZE chars are stored.
	If the character CONSOLE_INPUT_NEWLINE_CHAR is seen, then return CONSOLE_RC_OK if no overflow, else CONSOLE_ERROR_INPUT_OVERFLOW.
	In either case the buffer is nul terminated, but not all chars will have been stored on overflow. */
console_rc_t consoleAccept(char c);

// Followint functions are for implementing commands. Do not use unless in a recogniser function called by the console.

// Call on error, thanks to the magic of longjmp() it will return to the last setjmp with the error code.
void console_raise(console_rc_t rc);

// Error handling in commands.
void console_verify_can_pop(console_small_uint_t n);
void console_verify_can_push(console_small_uint_t n);
void console_verify_bounds(console_small_uint_t idx, console_small_uint_t size);

// Stack primitives.
console_int_t console_u_get(console_small_uint_t i); // 0 is TOS, 1 is NOS, ...
console_int_t* console_u_tos_(void);
#define console_u_tos() (*console_u_tos_())
console_int_t* console_u_nos_(void);
#define console_u_nos() (*console_u_nos_())
console_small_uint_t console_u_depth(void);
console_int_t console_u_pop(void);
void console_u_push(console_int_t x);
void console_u_clear(void);

/* Some helper macros for commands. */
#define console_binop(op_)	{ const console_int_t rhs = console_u_pop(); console_u_tos() = console_u_tos() op_ rhs; } 	// Implement a signed binary operator.
#define console_u_binop(op_)	{ \
  const console_uint_t rhs = (console_uint_t)console_u_pop(); \
  console_u_tos() = (console_int_t)((console_uint_t)console_u_tos() op_ rhs);	\
} 	// Implement an unsigned binary operator.
#define console_u_unop(op_)	{ console_u_tos() = (console_int_t)(op_ (console_uint_t)console_u_tos()); }											// Implement a signed unary operator.
#define console_unop(op_)	{ console_u_tos() = op_ console_u_tos(); }											// Implement a signed unary operator.

// Following functions are exposed for testing only.
//

// Return input buffer, only valid when consoleAccept() has not returned PENDING.
char* consoleAcceptBuffer(void);

/* Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
	The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
	All characters in the string are hashed even non-printable ones. */
uint16_t console_hash(const char* str);

#ifdef __cplusplus
}
#endif

#endif // CONSOLE_H__

