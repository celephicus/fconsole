#ifndef CONSOLE_INTERNALS_H__
#define CONSOLE_INTERNALS_H__

// Struct to hold the console interpreter's state.
#include <setjmp.h>
typedef struct {
	console_cell_t dstack[CONSOLE_DATA_STACK_SIZE];
	console_cell_t* sp;
	jmp_buf jmpbuf;				// How we do aborts.
} console_context_t;

extern console_context_t g_console_ctx;

// Call on error, thanks to the magic of longjmp() it will return to the last setjmp with the error code.
void console_raise(console_rc_t rc);

// Stack fills from top down.
#define CONSOLE_STACKBASE (&g_console_ctx.dstack[CONSOLE_DATA_STACK_SIZE])

// Predicates for push & pop.
#define console_can_pop(n_) (g_console_ctx.sp < (CONSOLE_STACKBASE - (n_) + 1))
#define console_can_push(n_) (g_console_ctx.sp >= &g_console_ctx.dstack[0 + (n_)])

// Error handling in commands.
void console_verify_can_pop(uint8_t n);
void console_verify_can_push(uint8_t n);

// Stack primitives.
console_cell_t console_u_pick(uint8_t i);
console_cell_t& console_u_tos();
console_cell_t& console_u_nos();
console_cell_t console_u_depth();
console_cell_t console_u_pop();
void console_u_push(console_cell_t x);
void console_u_clear();

/* Some helper macros for commands. */
#define console_binop(op_)	{ const console_cell_t rhs = console_u_pop(); console_u_tos() = console_u_tos() op_ rhs; } 	// Implement a binary operator.
#define console_unop(op_)	{ console_u_tos() = op_ console_u_tos(); }											// Implement a unary operator.

/* Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
	The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
	All characters in the string are hashed even non-printable ones. */
uint16_t console_hash(const char* str);

/* Recognisers are little parser functions that can turn a string into a value or values that are pushed onto the stack. They return 
	false if they cannot parse the input string. If they do parse it, they might call raise() if they cannot push a value onto the stack. */
typedef bool (*console_recogniser)(char* cmd);

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

#endif // CONSOLE_INTERNALS_H__
