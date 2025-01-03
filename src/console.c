// *** #include <Arduino.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "console.h"

// Clever macro to allow checking sizeof at compile time, _after_ preprocessing.
#define STATIC_ASSERT(expr_) extern int error_static_assert_fail__[(expr_) ? 1 : -1] __attribute__((unused))

// Is an integer type signed, works for chars as well.
#define utilsIsTypeSigned(T_) (((T_)(-1)) < (T_)0)

// And check for compatibility of the two console integral types. If this is wrong so much will break in surprising ways.
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wtype-limits"

STATIC_ASSERT(sizeof(console_int_t) == sizeof(console_uint_t));
STATIC_ASSERT(utilsIsTypeSigned(console_int_t));
STATIC_ASSERT(!utilsIsTypeSigned(console_uint_t));

// The console type must be able to represent a pointer.
STATIC_ASSERT(sizeof(void*) == sizeof(console_uint_t));

#pragma GCC diagnostic pop

// Unused static functions are OK. The linker will remove them.
// #pragma GCC diagnostic ignored "-Wunused-function"

// Struct to hold the console interpreter's state.
typedef struct {
	console_int_t dstack[CONSOLE_DATA_STACK_SIZE];	// Our stack, grows down in memory.
	console_int_t* sp;								// Stack pointer, points to topmost item. 
	jmp_buf jmpbuf;									// How we do aborts.
	const console_recogniser_func* recognisers;		// List of recogniser functions in PROGMEM. 
} console_context_t;

static console_context_t f_console_ctx;

// Stack fills from top down.
#define CONSOLE_STACKBASE (&f_console_ctx.dstack[CONSOLE_DATA_STACK_SIZE])

// Predicates for push & pop.
#define console_can_pop(n_) (f_console_ctx.sp < (CONSOLE_STACKBASE - (n_) + 1))
#define console_can_push(n_) (f_console_ctx.sp >= &f_console_ctx.dstack[0 + (n_)])

// Call on error, thanks to the magic of longjmp() it will return to the last setjmp with the error code.
void console_raise(console_rc_t rc) {
	longjmp(f_console_ctx.jmpbuf, rc);
}

// Error handling in commands.
void console_verify_can_pop(console_small_uint_t n) { if (!console_can_pop(n)) console_raise(CONSOLE_RC_ERR_DSTK_UNF); }
void console_verify_can_push(console_small_uint_t n) { if (!console_can_push(n)) console_raise(CONSOLE_RC_ERR_DSTK_OVF); }
void console_verify_bounds(console_small_uint_t idx, console_small_uint_t size) { if (idx >= size) console_raise(CONSOLE_RC_ERR_BAD_IDX); }

// Stack primitives.
console_int_t console_u_get(console_small_uint_t i)	{ console_verify_bounds(i, console_u_depth()); return f_console_ctx.sp[i]; }
console_int_t* console_u_tos_(void) 		{ console_verify_can_pop(1); return f_console_ctx.sp; }
console_int_t* console_u_nos_(void)			{ console_verify_can_pop(2); return f_console_ctx.sp + 1; }
console_small_uint_t console_u_depth(void)	{ return (console_small_uint_t)(CONSOLE_STACKBASE - f_console_ctx.sp); }
console_int_t console_u_pop(void) 			{ console_verify_can_pop(1); return *(f_console_ctx.sp++); }
void console_u_push(console_int_t x) 		{ console_verify_can_push(1); *--f_console_ctx.sp = x; }
void console_u_clear(void)					{ f_console_ctx.sp = CONSOLE_STACKBASE; }

// Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
// The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
// All characters in the string are hashed even non-printable ones.
#define HASH_START (5381)
#define HASH_MULT (33)
uint16_t console_hash(const char* str) {
	uint16_t h = HASH_START;
	char c;
	while ('\0' != (c = *str++)) {
		if ((c >= 'a') && (c <= 'z')) 	// Normalise letter case to UPPER CASE.
			c -= 'a' - 'A';
		h = (h * HASH_MULT) ^ (uint16_t)c;
	}
	return h;
}

// Convert a single character in range [0-9a-zA-Z] to a number up to 35. A large value (255) is returned on error.
static console_small_uint_t convert_digit(char c) {
	if ((c >= '0') && (c <= '9'))
		return (console_small_uint_t)c - '0';
	else if ((c >= 'a') && (c <= 'z'))
		return (console_small_uint_t)c -'a' + 10;
	else if ((c >= 'A') && (c <= 'Z'))
		return (console_small_uint_t)c -'A' + 10;
	else
		return 255;
}

// Convert an unsigned number of any base up to 36. Return true on success.
static bool convert_number(console_uint_t* number, console_small_uint_t base, const char* str) {
	if ('\0' == *str)		// If string is empty then fail.
		return false;

	*number = 0;
	while ('\0' != *str) {
		const console_small_uint_t digit = convert_digit(*str++);
		if (digit >= base)
			return false;		   /* Cannot convert with current base. */

		// Accumulate digits. Signal overflow if previous accumulator value non-zero _AND_ value does not increase.
		// We need the non-zero test to catch the case of all 1's in accumulator with an extra 'f' in hex.
		// This was not getting caught.
		const console_uint_t old_number = *number;
		*number = *number * base + digit;
		if ((old_number > (console_uint_t)0U) && (old_number >= *number))		
			console_raise(CONSOLE_RC_ERR_NUM_OVF);
	}

	return true;		// If we get here then it must have worked.
}

// Recognisers
//

// Regogniser for signed/unsigned decimal numbers.
bool console_r_number_decimal(char* cmd) {
	console_uint_t result;
	char sign;

	/* Check leading character for sign. */
	if (('-' == *cmd) || ('+' == *cmd))
		sign = *cmd++;
	else
		sign = ' ';

	/* Do conversion. */
	if (!convert_number(&result, 10, cmd))
		return false;

	/* Check overflow. */
	switch (sign) {
	default:
	case '+':		/* Unsigned, already checked for overflow. */
		break;
	case ' ':		/* Signed positive number. */
		if (result > (console_uint_t)CONSOLE_INT_MAX)
			console_raise(CONSOLE_RC_ERR_NUM_OVF);
		break;
	case '-':		/* Signed negative number. */
		if (result > ((console_uint_t)CONSOLE_INT_MIN))
			console_raise(CONSOLE_RC_ERR_NUM_OVF);
		result = (console_uint_t)-(console_int_t)result;
		break;
	}

	// Success.
	console_u_push((console_int_t)result);
	return true;
}

// Recogniser for hex numbers preceded by a '$'.
bool console_r_number_hex(char* cmd) {
	if ('$' != *cmd)
		return false;

	console_uint_t result;
	if (!convert_number(&result, 16, &cmd[1]))
		return false;

	// Success.
	console_u_push((console_int_t)result);
	return true;
}

// Convert 2 hex digits.
static bool convert_2_hex(const char* s, uint8_t* num) {
	const console_small_uint_t digit_1 = convert_digit(s[0]);
	if (digit_1 >= 16)
		return false;

	const console_small_uint_t digit_2 = convert_digit(s[1]);
	if (digit_2 >= 16)
		return false;
	*num = (digit_1 << 4) | digit_2;
	return true;
}

// String with a leading '"' pushes address of string which is zero terminated.
bool console_r_string(char* cmd) {
	if ('"' != cmd[0])
		return false;

	const char *rp = &cmd[1];		// Start reading from first char past the leading '"'.
	char *wp = &cmd[0];				// Write output string back into input buffer.

	while ('\0' != *rp) {			// Iterate over all chars...
		if ('\\' != *rp)			// Just copy all chars, the input routine makes sure that they are all printable. But
			*wp = *rp;
		else {
			rp += 1;				// On to next char.
			switch (*rp) {
				case 'n': *wp = '\n'; break;		// Common escapes.
				case 'r': *wp = '\r'; break;
				case '\0': goto exit;				// A '\' with no character is ignored.
				default: 							// Might be a hex character escape.
					if (convert_2_hex(rp, (uint8_t*)wp)) 	// Convert two hex digits.
						rp += 1;					// It worked, consume extra char from input.
					else
						*wp = *rp;					// Not hex, just copy the first char, this is how we do ' ' & '\'.
				break;
			}
		}
		wp += 1;
		rp += 1;
	}
exit:	*wp = '\0';									// Terminate string in input buffer.
	console_u_push((console_int_t)&cmd[0]);   		// Push address we started writing at.
	return true;
}

// Hex string with a leading '&', then n pairs of hex digits, pushes address of length of string, then binary data.
// So `&1aff01' will push a pointer to memory 03 1a ff 01.
bool console_r_hex_string(char* cmd) {
	unsigned char* len_ptr = (unsigned char*)cmd; 		// Leave space for length of counted string.
	if ('&' != *cmd++)
		return false;

	unsigned char* out_ptr = (unsigned char*)cmd; 	// We write the converted number back into the input buffer.
	while ('\0' != *cmd) {
		if (!convert_2_hex(cmd, out_ptr))			// Do conversion.
			return false;							// Bail on error;
		cmd += 2;
		out_ptr += 1;
	}
	*len_ptr = (unsigned char)(out_ptr - len_ptr) - 1; 		// Store length, looks odd, using len as a pointer and a value.
	if (0 == *len_ptr)
		return false;									// Zero length string is an error.
	console_u_push((console_int_t)len_ptr);				// Push _address_.
	return true;
}

// Essential commands that will always be required
bool console_cmds_builtin(char* cmd) {
	switch (console_hash(cmd)) {
		case /** . (d - ) Pop and print as signed decimal. **/ 0xb58b: consolePrint(CONSOLE_PRINT_SIGNED, console_u_pop()); break;
		case /** U. (u - ) Pop and print as unsigned decimal, with leading `+'. **/ 0x73de: consolePrint(CONSOLE_PRINT_UNSIGNED, console_u_pop()); break;
		case /** $. (u - ) Pop and print as 4 hex digits with leading `$'. **/ 0x658f: consolePrint(CONSOLE_PRINT_HEX, console_u_pop()); break;
		case /** ." (s - ) Pop and print string. **/ 0x66c9: consolePrint(CONSOLE_PRINT_STR, console_u_pop()); break; 		
		case /** DEPTH ( - u) Push stack depth. **/ 0xb508: console_u_push(console_u_depth()); break;					
		case /** CLEAR ( ... - <empty>) Remove all items from stack. **/ 0x9f9c: console_u_clear(); break;									
		case /** DROP (x - ) Remove top item from stack. **/ 0x5c2c: console_u_pop(); break;										
		case /** HASH (s - u) Pop string and push hash value. **/ 0x90b7: { console_u_tos() = console_hash((const char*)console_u_tos()); } break;	
		default: return false;
	}
	return true;
}

// Some "useful" commands used for testing and examples.
bool console_cmds_example(char* cmd) {
	switch (console_hash(cmd)) {
		case /** + (x1 x2 - x3) Add: x3 = x1 + x2. **/ 0xb58e: console_binop(+); break;
		case /** - (x1 x2 - x3) Subtract: x3 = x1 - x2. **/ 0xb588: console_binop(-); break;
		case /** * (d1 d2 - d3) Signed multiply: d3 = d1 * d2. **/ 0xb58f: console_binop(*); break;
		case /** RSHIFT (u1 n - u2) Logical bitwise shift right: u2 = u1 >> n. **/ 0x6b97: console_u_binop(>>); break;
		case /** / (d1 d2 - d3) Signed dvide: d3 = d1 / d2. **/ 0xb58a: {
			const console_int_t rhs = console_u_pop(); if (0 == rhs) console_raise(CONSOLE_RC_ERR_DIV_ZERO); 
			console_u_tos() = console_u_tos() / rhs;
		} break;
		case /** U/ (u1 u2 - u3) Unsigned divide: u3 = u1 / u2. **/ 0x73df: {
			const console_uint_t rhs = (console_uint_t)console_u_pop(); if ((console_uint_t)0 == rhs) console_raise(CONSOLE_RC_ERR_DIV_ZERO); 
			console_u_tos() = (console_int_t)((console_uint_t)console_u_tos() / rhs);
		} break;
		case /** NEGATE (d1 - d2) Negate signed value: d2 = -d1. **/ 0x7a79: console_unop(-); break;
		case /** # ( - ) Comment, rest of input ignored. **/ 0xb586: console_raise(CONSOLE_RC_STAT_IGN_EOL); break;
		case /** RAISE (i - ) Raise value as exception. **/ 0x4069: console_raise((console_rc_t)console_u_pop()); break;
		case /** EXIT ( - ?) Exit console. **/ 0xc745: console_raise(CONSOLE_RC_ERR_USER); break;	// Custom exception.
		case /** PICK (u - x) Copy stack item by index. **/ 0x13b4: console_u_tos() = console_u_get((console_small_uint_t)console_u_tos()+1); break;
		case /** OVER (x1 x2 - x1 x2 x1) Copy second stack item. **/ 0x398b: console_u_push(console_u_nos()); break;
		case /** PRINT (x i - ) Call consolePrint(i, x). **/ 0x47b4: { uint8_t opt = (uint8_t)console_u_pop(); consolePrint(opt, console_u_pop()); } break;
		default: return false;
	}
	return true;
}

// Optional help commands.
#ifdef CONSOLE_WANT_HELP

#include "console_help.autogen.inc"

bool console_cmds_help(char* cmd) {
	switch (console_hash(cmd)) {
		case /** ??HELP ( - ) Print (wordy) help for all commands. **/ 0xb0b4: {
			const char* const * hh = &help_cmds[0];
			for (console_small_uint_t i = 0; i < sizeof(help_cmds)/sizeof(help_cmds[0]); i += 1, hh += 1) {
				consolePrint(CONSOLE_PRINT_NEWLINE, 0);
				consolePrint(CONSOLE_PRINT_STR_P|CONSOLE_PRINT_NO_SEP, (console_int_t)CONSOLE_READ_PTR(hh));
			}
		} break;
		case /** ?HELP ( - ) Print list of all commands. **/ 0x74cb: {
			const char* const * hh = &help_cmds[0];
			for (console_small_uint_t i = 0; i < sizeof(help_cmds)/sizeof(help_cmds[0]); i += 1, hh += 1) {
				const char* str = (const char*)CONSOLE_READ_PTR(hh);
				char c;
				while ((' ' != (c = (char)CONSOLE_READ_BYTE(str++))) && ('\0' != c))
					consolePrint(CONSOLE_PRINT_CHAR|CONSOLE_PRINT_NO_SEP, c);
				consolePrint(CONSOLE_PRINT_CHAR|CONSOLE_PRINT_NO_SEP, ' ');
			}
		} break;
		case /** HELP (s - ) Search for help on given command. **/ 0x7d54: {
			const uint16_t cmd_hash = console_hash((const char*)console_u_pop());
			const uint16_t* hh = &help_hashes[0];
			for (console_small_uint_t i = 0; i < sizeof(help_hashes)/sizeof(help_hashes[0]); i += 1, hh += 1) {
				if((uint16_t)CONSOLE_READ_PTR(hh) == cmd_hash) {
					consolePrint(CONSOLE_PRINT_STR_P, (console_int_t)CONSOLE_READ_PTR(&help_cmds[i]));
					return true;
				}
			}
			console_raise(CONSOLE_RC_ERR_BAD_CMD);
		} break;
		default: return false;
	}
	return true;
}
#else
bool console_cmds_help(char* cmd) {	return false; }
#endif // CONSOLE_WANT_HELP

// Example output routines.
#ifdef CONSOLE_WANT_PRINT_FUNC
void consolePrintStream(FILE *stream, console_small_uint_t opt, console_int_t x) {
	switch (opt & ~(CONSOLE_PRINT_NO_LEAD|CONSOLE_PRINT_NO_SEP)) {
		case CONSOLE_PRINT_NEWLINE:		fprintf(stream, CONSOLE_OUTPUT_NEWLINE_STR; (void)x; return;	// No separator.
		default:						(void)x; return;						// Ignore, print nothing.
		case CONSOLE_PRINT_SIGNED:		fprintf(stream, "%" CONSOLE_PRINTF_FMT_MOD "d", x); break;
		case CONSOLE_PRINT_UNSIGNED:	fprintf(stream, "%s%" CONSOLE_PRINTF_FMT_MOD "u", (opt & CONSOLE_PRINT_NO_LEAD) ? "" : "+", (console_uint_t)x); break;
		case CONSOLE_PRINT_HEX:			fprintf(stream, "%s%016" CONSOLE_PRINTF_FMT_MOD "X", (opt & CONSOLE_PRINT_NO_LEAD) ? "" : "$", (console_uint_t)x); break;
		case CONSOLE_PRINT_HEX2:		fprintf(stream, "%s%02"  CONSOLE_PRINTF_FMT_MOD "X",  (opt & CONSOLE_PRINT_NO_LEAD) ? "" : "$", (console_uint_t)x & 0xff); break;
		case CONSOLE_PRINT_STR_P:		/* Fall through... */
		case CONSOLE_PRINT_STR:			fprintf(stream, "%s",(const char*)x); break;
		case CONSOLE_PRINT_CHAR:		fprintf(stream, "%c", (char)x); break;
	}
	if (!(opt & CONSOLE_PRINT_NO_SEP))	fprintf(stream, " ");								// Print a space.
	//fprintf(stream, "*");
}
#endif

static bool is_whitespace(char c) { return (' ' == c) || ('\t' == c); }
static bool is_nul(char c) { return ('\0' == c); }

// Execute a single command from a string
static console_rc_t execute(char* cmd) {
	// Try all recognisers in turn until one works.
	const console_recogniser_func* rp = f_console_ctx.recognisers;
	while (1) {
		const console_recogniser_func r = (console_recogniser_func)CONSOLE_READ_PTR(rp++);
		if (NULL == r)										// Exit at end.
			break;
		if (r(cmd))											// Call recogniser function, returns true on success.
			return CONSOLE_RC_OK;	 						// Recogniser succeeded.
	}
	return CONSOLE_RC_ERR_BAD_CMD;
}

// External functions.

void consoleInit(const console_recogniser_func* r_list) {
	f_console_ctx.recognisers = r_list;
	console_u_clear();
}

console_rc_t consoleProcess(char* str, char** current) {
	char* volatile cmd;				// Necessary to avoid warning from setjmp clobber variables optimised into registers.
	char* volatile vstr = str;
	console_rc_t command_rc;
	
	// Establish a point where raise will go to when raise() is called.
	command_rc = (console_rc_t)setjmp(f_console_ctx.jmpbuf); 
	if (CONSOLE_RC_OK != command_rc) 	// On a raise we get here, normal program flow will return zero.
		goto error;						// Handle error and bail.
		
	// Iterate over input, breaking into words.
	while (1) {
		while (is_whitespace(*vstr)) 									// Advance past leading spaces.
			vstr += 1;

		if (is_nul(*vstr))												// Stop at end.
			break;

		// Record start & advance until we see a space.
		cmd = vstr;
		while ((!is_whitespace(*vstr)) && (!is_nul(*vstr)))
			vstr += 1;

		if (!is_nul(*vstr))								// If there was NOT already a nul at the end of this string...
			*vstr++ = '\0';								// Terminate white space delimited command and advance to next char.

		command_rc = execute(cmd);						// Try to execute command.
		if (CONSOLE_RC_OK != command_rc) {				// Bail on error.
error:		if (command_rc < CONSOLE_RC_OK) // Negative error codes are not really errors, used to implement things like comments. 
				return CONSOLE_RC_OK;		// Fake no error to caller.

			if (NULL != current)	// Update user pointer to point to last command executed, good for error messages.
				*current = cmd;
			return command_rc;		// Return error code to caller. 
		}
	}

	return CONSOLE_RC_OK;
}

// Print description of error code.
const char* consoleGetErrorDescription(console_rc_t err) {
	switch(err) {
		case CONSOLE_RC_ERR_NO_CHEESE: 	return CONSOLE_PSTR(" ++?????++ Out of Cheese Error. Redo From Start");
		case CONSOLE_RC_ERR_NUM_OVF: 	return CONSOLE_PSTR("number overflow");
		case CONSOLE_RC_ERR_DSTK_UNF: 	return CONSOLE_PSTR("stack underflow");
		case CONSOLE_RC_ERR_DSTK_OVF: 	return CONSOLE_PSTR("stack overflow");
		case CONSOLE_RC_ERR_ACC_OVF: 	return CONSOLE_PSTR("input buffer overflow");
		case CONSOLE_RC_ERR_BAD_IDX:	return CONSOLE_PSTR("index out of range");
		case CONSOLE_RC_ERR_BAD_CMD: 	return CONSOLE_PSTR("unknown command");
		case CONSOLE_RC_ERR_DIV_ZERO: 	return CONSOLE_PSTR("divide by zero");
		default: 						return CONSOLE_PSTR("??");
	}
}

// Input functions.
// State for consoleAccept(). Done seperately as if not used the linker will remove it.
typedef struct {
	char inbuf[CONSOLE_INPUT_BUFFER_SIZE + 1];
	console_small_uint_t inbidx;
} accept_context_t;
static accept_context_t f_accept_context;

void consoleAcceptClear() {
	f_accept_context.inbidx = 0;
}

console_rc_t consoleAccept(char c) {
	bool overflow = (f_accept_context.inbidx >= sizeof(f_accept_context.inbuf));

	if (CONSOLE_INPUT_NEWLINE_CHAR == c) {
		f_accept_context.inbuf[f_accept_context.inbidx] = '\0';
		consoleAcceptClear();
		return overflow ? CONSOLE_RC_ERR_ACC_OVF : CONSOLE_RC_OK;
	}
	else {
		if ((c >= ' ') && (c < (char)0x7f)) {	 // Is is printable?
			if (!overflow)
				f_accept_context.inbuf[f_accept_context.inbidx++] = c;
		}
		return CONSOLE_RC_STAT_ACC_PEND;
	}
}
char* consoleAcceptBuffer() { return f_accept_context.inbuf; }
