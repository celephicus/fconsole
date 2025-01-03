#include <Arduino.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>		// Need this for implementing exceptions.

#include "console.h"

// When something must be true at compile time...
#define STATIC_ASSERT(expr_) extern int error_static_assert_fail__[(expr_) ? 1 : -1] __attribute__((unused))

// Is an integer type signed.
#define isTypeSigned(T_) (((T_)(-1)) < (T_)0)

// And check for compatibility of the two cell types.
STATIC_ASSERT(sizeof(console_cell_t) == sizeof(console_ucell_t));
STATIC_ASSERT(isTypeSigned(console_cell_t));
STATIC_ASSERT(!isTypeSigned(console_ucell_t));

// We must have macros PSTR that places a const string into const memory, and READ_FUNC_PTR that deferences a pointer to a function in Flash.
#if defined(AVR)
 #include <avr/pgmspace.h>	// Takes care of PSTR().
 #define CONSOLE_READ_FUNC_PTR(x_) pgm_read_word(x_)		// 16 bit target.
#elif defined(ESP32 )
 #include <pgmspace.h>	// Takes care of PSTR().
 #define CONSOLE_READ_FUNC_PTR(x_) pgm_read_dword(x_)		// 32 bit target.
#else
 #define PSTR(str_) (str_)
 #define CONSOLE_READ_FUNC_PTR(x_) (*(x_))					// Generic target.
#endif

// Stack size, we don't need much.
#define CONSOLE_DATA_STACK_SIZE (8)

// Input buffer size.
#define CONSOLE_INPUT_BUFFER_SIZE 40

// Character to signal EOL for input string.
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

/* Get max/min for types. This only works because we assume two's complement representation and we have checked that the signed & 
	unsigned types are compatible. */
#define CONSOLE_UCELL_MAX ((console_ucell_t)(~(console_ucell_t)(0)))
#define CONSOLE_CELL_MAX ((console_cell_t)(CONSOLE_UCELL_MAX >> 1))
#define CONSOLE_CELL_MIN ((console_cell_t)(~(CONSOLE_UCELL_MAX >> 1)))

// We must be able to fit a pointer into a cell.
STATIC_ASSERT(sizeof(void*) <= sizeof(console_ucell_t));

// Unused static functions are OK. The linker will remove them.
#pragma GCC diagnostic ignored "-Wunused-function"

// All console interpreter's state is in a struct for easy viewing on a debugger.
static struct {
	Stream* s;										// Stream object for IO.
	console_cell_t dstack[CONSOLE_DATA_STACK_SIZE];	// Our stack, grows down in memory.
	console_cell_t* sp;								// Stack pointer, points to topmost item.
	jmp_buf jmpbuf;									// How we do aborts.
	console_recogniser_func local_r;				// Local recogniser function.
	uint8_t flags;									// Modify behaviour of console, prompt, echo...
	char inbuf[CONSOLE_INPUT_BUFFER_SIZE + 1];		// Input buffer.
	uint8_t inbidx;									// Next free location in input buffer.
} f_ctx;

// Make input buffer ready to accept new data.
static void console_accept_clear() {
	f_ctx.inbidx = 0;
}

// Stack fills from top down.
#define CONSOLE_STACKBASE (&f_ctx.dstack[CONSOLE_DATA_STACK_SIZE])

// Predicates for push & pop.
#define console_can_pop(n_) (f_ctx.sp < (CONSOLE_STACKBASE - (n_) + 1))
#define console_can_push(n_) (f_ctx.sp >= &f_ctx.dstack[0 + (n_)])

// Use in a command to verify stack.
static void console_verify_can_pop(uint8_t n) { if (!console_can_pop(n)) consoleRaise(CONSOLE_RC_ERROR_DSTACK_UNDERFLOW); }
static void console_verify_can_push(uint8_t n) { if (!console_can_push(n)) consoleRaise(CONSOLE_RC_ERROR_DSTACK_OVERFLOW); }

// Use in a command to verify that an index is within bounds.
#define console_verify_bounds(_x, _size) do { if ((_x) >= (_size)) consoleRaise(CONSOLE_RC_ERROR_INDEX_OUT_OF_RANGE); } while (0)

// Stack primitives.
console_cell_t consoleStackPick(uint8_t i)	{ console_verify_bounds(i+1, consoleStackDepth()); return f_ctx.sp[i]; }
console_cell_t& consoleStackTos() 			{ console_verify_can_pop(1); return *f_ctx.sp; }
console_cell_t& consoleStackNos() 			{ console_verify_can_pop(2); return *(f_ctx.sp + 1); }
console_cell_t consoleStackDepth() 			{ return (CONSOLE_STACKBASE - f_ctx.sp); }
console_cell_t consoleStackPop() 			{ console_verify_can_pop(1); return *(f_ctx.sp++); }
void consoleStackPush(console_cell_t x) 	{ console_verify_can_push(1); *--f_ctx.sp = x; }
void consoleStackClear()					{ f_ctx.sp = CONSOLE_STACKBASE; }

/* Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
	The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
	All characters in the string are hashed even non-printable ones. */
#define HASH_START 5381U
#define HASH_MULT 33U
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
static uint8_t convert_digit(char c) {
	if ((c >= '0') && (c <= '9'))
		return (int8_t)c - '0';
	else if ((c >= 'a') && (c <= 'z'))
		return (int8_t)c -'a' + 10;
	else if ((c >= 'A') && (c <= 'Z'))
		return (int8_t)c -'A' + 10;
	else
		return 255;
}

// Convert an unsigned number of any base up to 36. Writes value to `number' and returns true on success.
static bool convert_number(console_ucell_t* number, console_cell_t base, const char* str) {
	if ('\0' == *str)		// If string is empty then fail.
		return false;

	*number = 0;
	while ('\0' != *str) {
		const uint8_t digit = convert_digit(*str++);
		if (digit >= base)
			return false;		   /* Cannot convert with current base. */

		const console_ucell_t old_number = *number;
		*number = *number * base + digit;
		if (old_number > *number)		// Magnitude change signals overflow.
			consoleRaise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
	}

	return true;		// If we get here then it must have worked.
}

/* Takes a character and if printable saves in input buffer. Will not overflow buffer, but chars past the size of the buffer are lost.
	On a newline char terminates the buffer with a nul char and returns an OK or overflow status. */
static console_rc_t console_accept(char c) {
	bool overflow = (f_ctx.inbidx >= sizeof(f_ctx.inbuf));

	if (CONSOLE_INPUT_NEWLINE_CHAR == c) {
		f_ctx.inbuf[f_ctx.inbidx] = '\0';
		console_accept_clear();
		return overflow ? CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW : CONSOLE_RC_OK;
	}
	else {
		if ((c >= ' ') && (c < (char)0x7f)) {	 // Is is printable?
			if (!overflow)
				f_ctx.inbuf[f_ctx.inbidx++] = c;
		}
		return CONSOLE_RC_STATUS_ACCEPT_PENDING;
	}
}

// Get description of error code.
static const char* get_error_desc(console_rc_t err) {
	switch(err) {
		case CONSOLE_RC_ERROR_NUMBER_OVERFLOW: return PSTR("number overflow");
		case CONSOLE_RC_ERROR_DSTACK_UNDERFLOW: return PSTR("stack underflow");
		case CONSOLE_RC_ERROR_DSTACK_OVERFLOW: return PSTR("stack overflow");
		case CONSOLE_RC_ERROR_UNKNOWN_COMMAND: return PSTR("unknown command");
		case CONSOLE_RC_ERROR_INDEX_OUT_OF_RANGE: return PSTR("index out of range");
		case CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW: return PSTR("input buffer overflow");
		default: return PSTR("???");
	}
}

// Some helper macros for commands. 
#define console_binop(op_)	{ const console_cell_t rhs = console_u_pop(); console_u_tos() = console_u_tos() op_ rhs; } 	// Implement a binary operator.
#define console_unop(op_)	{ console_u_tos() = op_ console_u_tos(); }											// Implement a unary operator.

/* Recogniser for signed/unsigned decimal numbers. Will raise CONSOLE_RC_ERROR_NUMBER_OVERFLOW if numbers are to big in magnitude.
	Examples for 16 bit cells:
	 unsigned: +0 +1 +65535
	 signed: 0 -1 1 -32768 32767 */
bool console_r_number_decimal(char* cmd) {
	console_ucell_t result;
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
		if (result > (console_ucell_t)CONSOLE_CELL_MAX)
			consoleRaise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		break;
	case '-':		/* Signed negative number. */
		if (result > ((console_ucell_t)CONSOLE_CELL_MIN))
			consoleRaise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		result = (console_ucell_t)-(console_cell_t)result;
		break;
	}

	// Success.
	consoleStackPush(result);
	return true;
}

/* Recogniser for hex numbers preceded by a '$'. Will raise CONSOLE_RC_ERROR_NUMBER_OVERFLOW if numbers are to big in magnitude.
	Examples for 16 bit cells: $0 $1 $ab $FFFF */
bool console_r_number_hex(char* cmd) {
	if ('$' != *cmd)
		return false;

	console_ucell_t result;
	if (!convert_number(&result, 16, &cmd[1]))
		return false;

	// Success.
	consoleStackPush(result);
	return true;
}

/* Recogniser for strings with a leading '"' pushes address of string which is zero terminated. The string remains in the input buffer so it must
	be used before another line of data is input. 
	Any character can be entered as a hex escape e.g `\20' iis a space. */
bool console_r_string(char* cmd) {
	if ('"' != cmd[0])
		return false;

	const char *rp = &cmd[1];		// Start reading from first char past the leading '"'.
	char *wp = &cmd[0];				// Write output string back into input buffer.

	while ('\0' != *rp) {			// Iterate over all chars...
		if ('\\' != *rp)			// Just copy all chars, the input routine makes sure that they are all printable. But
			*wp++ = *rp;
		else {
			rp += 1;				// On to next char.
			switch (*rp) {
				case 'n': *wp++ = '\n'; break;						// Common escapes.
				case 'r': *wp++ = '\r'; break;
				case '\0': *wp++ = ' '; goto exit;					// Trailing '\' is a space, so exit loop now.
				default: {											// Might be a hex character escape.
					const uint8_t digit_1 = convert_digit(rp[0]); 	// A bit naughty, we might read beyond the end of the buffer
					const uint8_t digit_2 = convert_digit(rp[1]);
					if ((digit_1 < 16) && (digit_2 < 16)) {			// If two valid hex chars...
						*wp++ = (digit_1 << 4) | digit_2;
						rp += 1;
					}
					else											// Not valid hex...
						*wp++ = *rp;								// Just copy the first char, this is how we do '\'.
				} break;
			}
		}
		rp += 1;
	}
exit:	*wp = '\0';									// Terminate string in input buffer.
	consoleStackPush((console_cell_t)&cmd[0]);   	// Push address we started writing at.
	return true;
}

/* Recogniser for a hex string with a leading '&', then n pairs of hex digits, pushes address of bytecount, then binary data.
	So `&1aff01' will overwrite the inout string with bytes `03 1a ff 01' and push the address of the bytecount. */
bool console_r_hex_string(char* cmd) {
	unsigned char* len = (unsigned char*)cmd; // Leave space for length of counted string.
	if ('&' != *cmd++)
		return false;

	unsigned char* out_ptr = (unsigned char*)cmd; // We write the converted number back into the input buffer.
	while ('\0' != *cmd) {
		const uint8_t digit_1 = convert_digit(*cmd++);
		if (digit_1 >= 16)
			return false;

		const uint8_t digit_2 = convert_digit(*cmd++);
		if (digit_2 >= 16)
			return false;
		*out_ptr++ = (digit_1 << 4) | digit_2;
	}
	*len = out_ptr - len - 1; 					// Store length, looks odd, using len as a pointer and a value.
	consoleStackPush((console_cell_t)len);		// Push _address_.
	return true;
}

/* Recogniser for essential commands that will always be required. */
bool console_cmds_builtin(char* cmd) {
	switch (console_hash(cmd)) {
		case /** . **/ 0XB58B: consolePrint(CFMT_D, consoleStackPop()); break;		// Pop and print in signed decimal.
		case /** U. **/ 0X73DE: consolePrint(CFMT_U, consoleStackPop()); break;		// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0X658F: consolePrint(CFMT_X, consoleStackPop()); break; 	// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0X66C9: consolePrint(CFMT_STR, consoleStackPop()); break; 	// Pop and print string.
		case /** ?DEPTH **/ 0XB508: consolePrint(CFMT_D, consoleStackDepth()); break;	// Print stack depth.
		case /** CLEAR **/ 0X9F9C: consoleStackClear(); break;						// Clear stack so that it has zero depth.
		case /** DROP **/ 0X5C2C: consoleStackPop(); break;							// Remove top item from stack.
		case /** HASH **/ 0X90B7: { consoleStackTos() = console_hash((const char*)consoleStackTos()); } break;	// Pop string and push hash value.
		default: return false;
	}
	return true;
}

// Shim function to call user recogniser function that is in RAM, so it can't be accessed via a PROGMEM pointer. Stupid AVR!
static bool local_r(char* cmd) { return f_ctx.local_r(cmd); }

/* List of recognisers, the number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
	can have potentially any hash value so could look like commands. */
static const console_recogniser_func RECOGNISERS[] PROGMEM = {
	console_r_number_decimal,
	console_r_number_hex,
	console_r_string,
	console_r_hex_string,
	console_cmds_builtin,
	local_r,
	NULL
};

/* Execute a single command from a string. Sets up a handler for exceptions and will pass error code back to caller. */
static uint8_t execute(char* cmd) {
	// Establish a point where consoleRaise will go to if called.
	console_rc_t command_rc = (console_rc_t)setjmp(f_ctx.jmpbuf); 	// When called in normal execution it returns zero.
	if (CONSOLE_RC_OK != command_rc)								// We got an error, back to caller. 
		return command_rc;

	// Try all recognisers in turn until one works.
	const console_recogniser_func* rp = RECOGNISERS;
	console_recogniser_func r;
	while (NULL != (r = (console_recogniser_func)CONSOLE_READ_FUNC_PTR(rp++))) {	// Avoid dereferencing pointer twice.
		if (r(cmd))											// Call recogniser function.
			return CONSOLE_RC_OK;	 						// Recogniser succeeded.
	}
	return CONSOLE_RC_ERROR_UNKNOWN_COMMAND;
}

static console_rc_t console_process(char* str) {
	const char wsp[] = " \t";		// These chars separate tokens in input.
	
	// Start off getting first token (if any) from input buffer, separated by characters in second arg. 
	char* cmd_or_arg = strtok(str, wsp);
	
	while (NULL != cmd_or_arg) {
		// Execute parsed command and exit on any abort, so that we do not execute any more commands.
		const console_rc_t command_rc = execute(cmd_or_arg);
		
		// Note that no error is returned for any negative eror codes, which is used to implement comments with the	CONSOLE_RC_SIGNAL_IGNORE_TO_EOL code. 
		if (command_rc < (console_rc_t)CONSOLE_RC_OK)	// Cast to avoid gcc mixed enumeral/numeral compare warning. Who knew that enumerals lived in enums?
			 return CONSOLE_RC_OK;
			
		// Exit on abort code.
		if (command_rc > (console_rc_t)CONSOLE_RC_OK)	
			 return command_rc;
		
		// Get next command or argument from input buffer, note that pointer arg is NULL.
		cmd_or_arg = strtok(NULL, wsp);
	}
	return CONSOLE_RC_OK;
}

// External functions.
//

void consoleInit(console_recogniser_func r, Stream& s) {
	f_ctx.local_r = r;
	f_ctx.s = &s;
	consoleStackClear();
	console_accept_clear();
	// We do not set f_ctx.jmpbuf, there is no safe value.
}

void consoleRaise(console_rc_t rc) {
	/* TODO: Is using longjmp to transport the error code valid, I seem to remember reading somewhere that it should only be used as a logical flag.
		Maybe in the Stevens' Unix book? */
	longjmp(f_ctx.jmpbuf, rc);
}

void consolePrompt() {
	consolePrint(CFMT_STR_P, (console_cell_t)PSTR(CONSOLE_OUTPUT_NEWLINE_STR ">"));
}

console_rc_t consoleService() {
	while (f_ctx.s->available()) {								// If a character is available from serial port...
		const char c = f_ctx.s->read();
		console_rc_t rc = console_accept(c);					// Add it to the input buffer, will take care not to overflow.
		#if CONSOLE_ECHO_CHAR
		if (c >= ' ') f_ctx.s->write(c);
		#endif		
		if (rc >= CONSOLE_RC_OK) {								// On newline...
			f_ctx.s->print(F(" -> ")); 						// Seperator string for output.
			if (CONSOLE_RC_OK == rc)							// If accept has _NOT_ returned an error (probably overflow)...
				rc = console_process(f_ctx.inbuf);				// Process input string from input buffer filled by accept and record error code.
			if (CONSOLE_RC_OK != rc) {							// If all went well then we get an OK status code
				f_ctx.s->print(F("Error: ")); 					// Print error code:(
				f_ctx.s->print((const __FlashStringHelper *)get_error_desc(rc)); // Print error description.
				f_ctx.s->print(F(" : "));
				f_ctx.s->print(rc);								// Print error code as well.
			}
			consolePrompt();									// In any case print a newline and prompt ready for the next line of input.
			return rc;											// Exit, possibly leaving chars in the stream buffer to be read next time.
		}
	}
	return CONSOLE_RC_STATUS_ACCEPT_PENDING;
}

void consolePrint(uint8_t opt, console_cell_t x) {
	switch (opt & 0x3f) {
		case CFMT_NL:		f_ctx.s->print(F(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; return; 	// Newline with no separator.
		default:			(void)x; return;													// Ignore, print nothing.
		case CFMT_D:		f_ctx.s->print((console_cell_t)x, DEC); break;						// Signed decimal, possible leading '-', no '+'. 
		case CFMT_U:		if (!(opt & CFMT_M_NO_LEAD)) f_ctx.s->print('+');					// Unsigned decimal with leading '+'.
							f_ctx.s->print((console_ucell_t)x, DEC); break;
		case CFMT_U_D:		if (!(opt & CFMT_M_NO_LEAD))  f_ctx.s->print('+');					// Unsigned LONG decimal with leading '+'.
							f_ctx.s->print(*(uint32_t*)x, DEC); break;
		case CFMT_X_D:		if (!(opt & CFMT_M_NO_LEAD)) f_ctx.s->print('$');					// LONG hex with leading '$' & leading '0's.
							{ const uint32_t xx = *(uint32_t*)x;
							  consolePrint(CFMT_X|CFMT_M_NO_LEAD, (console_cell_t)(xx>>16));
							  consolePrint(CFMT_X|CFMT_M_NO_LEAD, (console_cell_t)xx);
							} break;
		case CFMT_X:		if (!(opt & CFMT_M_NO_LEAD)) f_ctx.s->print('$');					// Hex with leading '$' & leading '0's.
							for (console_ucell_t m = 0xf; CONSOLE_UCELL_MAX != m; m = (m << 4) | 0xf) {
								if ((console_ucell_t)x <= m)
									f_ctx.s->print('0');
							}
							f_ctx.s->print((console_ucell_t)x, HEX); break;
		case CFMT_STR:		f_ctx.s->print((const char*)x); break;								// String in RAM.
		case CFMT_STR_P:	f_ctx.s->print((const __FlashStringHelper*)x); break;				// String in PROGMEM.
		case CFMT_C:		f_ctx.s->print((char)x); break;										// Single character.
		case CFMT_X2: 		if ((console_ucell_t)x < 16) f_ctx.s->print('0');					// Hex byte with no leader.
							f_ctx.s->print((console_ucell_t)(x), HEX);
							break;
	}
	if (!(opt & CFMT_M_NO_SEP))
		f_ctx.s->print(' ');			// Print a trailing space.
}

