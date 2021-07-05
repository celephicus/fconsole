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
#define STACKBASE (&g_console_ctx.dstack[CONSOLE_DATA_STACK_SIZE])

// Predicates for push & pop.
#define console_can_pop(n_) (g_console_ctx.sp < (STACKBASE - (n_) + 1))
#define console_can_push(n_) (g_console_ctx.sp >= &g_console_ctx.dstack[0 + (n_)])

// Error handling in commands.
void console_verify_can_pop(uint8_t n);
void console_verify_can_push(uint8_t n);

// Stack primitives.
console_cell_t u_pick(uint8_t i);
console_cell_t* u_tos();
console_cell_t* u_nos();
console_cell_t u_depth();
console_cell_t u_pop();
void u_push(console_cell_t x);
void clear_stack();

/* Some helper macros for commands. */
#define console_binop(op_)	{ const console_cell_t rhs = u_pop(); *u_tos() = *u_tos() op_ rhs; } 	// Implement a binary operator.
#define console_unop(op_)	{ *u_tos() = op_ *u_tos(); }											// Implement a unary operator.

// Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
// The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
// All characters in the string are hashed even non-printable ones.
uint16_t console_hash(const char* str);

#endif // CONSOLE_INTERNALS_H__
