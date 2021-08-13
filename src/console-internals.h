#ifndef CONSOLE_INTERNALS_H__
#define CONSOLE_INTERNALS_H__

// Struct to hold the console interpreter's state.
#include <setjmp.h>
typedef struct {
	console_cell_t dstack[CONSOLE_DATA_STACK_SIZE];	// Our stack, grows down in memory.
	console_cell_t* sp;								// Stack pointer, points to topmost item. 
	jmp_buf jmpbuf;									// How we do aborts.
	const console_recogniser_func* recognisers;		// List of recogniser functions in PROGMEM. 

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
#define console_verify_bounds(_x, _size) do { if ((_x) >= (_size)) console_raise(CONSOLE_RC_ERROR_INDEX_OUT_OF_RANGE); } while (0)

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

#endif // CONSOLE_INTERNALS_H__
