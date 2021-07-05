#include <Arduino.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "console.h"
#include "console-internals.h"

// How many elements in an array?
#define ELEMENT_COUNT(x_) (sizeof(x_) / sizeof((x_)[0]))

// Unused static functions are OK. The linker will remove them.
#pragma GCC diagnostic ignored "-Wunused-function"

console_context_t g_console_ctx;	

// State for consoleAccept(). Done seperately as if not used the linker will remove it.
typedef struct {
	char inbuf[CONSOLE_INPUT_BUFFER_SIZE + 1];
	uint8_t inbidx;
} accept_context_t;
static accept_context_t f_accept_context;

// Call on error, thanks to the magic of longjmp() it will return to the last setjmp with the error code.
void console_raise(console_rc_t rc) {
	longjmp(g_console_ctx.jmpbuf, rc);
}

// Stack fills from top down.
#define STACKBASE (&g_console_ctx.dstack[CONSOLE_DATA_STACK_SIZE])

// Predicates for push & pop.
#define console_can_pop(n_) (g_console_ctx.sp < (STACKBASE - (n_) + 1))	
#define console_can_push(n_) (g_console_ctx.sp >= &g_console_ctx.dstack[0 + (n_)])

// Error handling in commands.
void console_verify_can_pop(uint8_t n) { if (!console_can_pop(n)) console_raise(CONSOLE_RC_ERROR_DSTACK_UNDERFLOW); }
void console_verify_can_push(uint8_t n) { if (!console_can_push(n)) console_raise(CONSOLE_RC_ERROR_DSTACK_OVERFLOW); }

// Stack primitives.
console_cell_t u_pick(uint8_t i) 	{ return g_console_ctx.sp[i]; }
console_cell_t* u_tos() 				{ console_verify_can_pop(1); return g_console_ctx.sp; }
console_cell_t* u_nos() 				{ console_verify_can_pop(2); return g_console_ctx.sp + 1; }
console_cell_t u_depth() 			{ return (STACKBASE - g_console_ctx.sp); } 
console_cell_t u_pop() 				{ console_verify_can_pop(1); return *(g_console_ctx.sp++); }
void u_push(console_cell_t x) 		{ console_verify_can_push(1); *--g_console_ctx.sp = x; }
void clear_stack()					{ g_console_ctx.sp = STACKBASE; }

// Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
// The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
// All characters in the string are hashed even non-printable ones.
#define HASH_START (5381)
#define HASH_MULT (33)
uint16_t console_hash(const char* str) {
	uint16_t h = HASH_START;
	char c;
	while ('\0' != (c = *str++)) {
		if ((c >= 'a') && (c <= 'z')) 
			c -= 'a' - 'A';
		h = (h * HASH_MULT) ^ (uint16_t)c;
	}
	return h;
}	 

// Convert a single character in range [0-9a-zA-Z] to a number up to 25. A large value (255) is returned on error. 
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

// Convert an unsigned number of any base up to 36. Return true on success.
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
			console_raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
	}
	
	return true;		// If we get here then it must have worked. 
}  

// Recognisers are little parser functions that can turn a string into a value or values that are pushed onto the stack. They return false if they cannot
//  parse the input string. If they do parse it, they might call raise() if they cannot push a value onto the stack.
typedef bool (*console_recogniser)(char* cmd);

/* Recogniser for signed/unsigned decimal number.
	The number format is as follows:
		An initial '-' flags the number as negative, the '-' character is illegal anywhere else in the word.
		An initial '+' flags the number as unsigned. In which case the range of the number is up to the maximum
		  _unsigned_ value for the type before overflow is flagged.
	Return values are good, bad, overflow.
 */
static bool console_r_number_decimal(char* cmd) {
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
			console_raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		break;
	case '-':		/* Signed negative number. */
		if (result > ((console_ucell_t)CONSOLE_CELL_MIN))
			console_raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		result = (console_ucell_t)-(console_cell_t)result;
		break;
	}

	// Success.
	u_push(result);
	return true;
}

/* Recogniser for hex numbers preceded by a '$'. Return values are good, bad, overflow. */
static bool console_r_number_hex(char* cmd) {
	if ('$' != *cmd) 
		return false;

	console_ucell_t result;
	if (!convert_number(&result, 16, &cmd[1]))
		return false;
	
	// Success.
	u_push(result);
	return true;
}

// String with a leading '"' pushes address of string which is zero terminated.
static bool console_r_string(char* cmd) {
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
				case 'n': *wp++ = '\n'; break;		// Common escapes.
				case 'r': *wp++ = '\r'; break;
				case '\0': *wp++ = ' '; goto exit;	// Trailing '\' is a space, so exit loop now. 
				default: 							// Might be a hex character escape.
				{
					const uint8_t digit_1 = convert_digit(rp[0]); // A bit naughty, we might read beyond the end of the buffer
					const uint8_t digit_2 = convert_digit(rp[1]);
					if ((digit_1 < 16) && (digit_2 < 16)) {		// If a valid hex char...
						*wp++ = (digit_1 << 4) | digit_2;
						rp += 1;
					}
					else
						*wp++ = *rp;								// It's not, just copy the first char, this is how we do ' ' & '\'. 
				}
				break;
			}
		}
		rp += 1;
	}
exit:	*wp = '\0';						// Terminate string in input buffer. 
	u_push((console_cell_t)&cmd[0]);   	// Push address we started writing at.
	return true;
}

// Hex string with a leading '&', then n pairs of hex digits, pushes address of length of string, then binary data.
// So `&1aff01' will push a pointer to memory 03 1a ff 01.
static bool console_r_hex_string(char* cmd) {
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
	*len = out_ptr - len - 1; 		// Store length, looks odd, using len as a pointer and a value.
	u_push((console_cell_t)len);	// Push _address_.
	return true;
}

// Example output routines.
#if 0
console_print(uint8_t s, console_cell_t x) {
	switch (s) {
		case CONSOLE_PRINT_NEWLINE:		printf(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; break;
		case CONSOLE_PRINT_SIGNED:		printf("%d ", x); break;
		case CONSOLE_PRINT_UNSIGNED:	printf("+%u ", (console_ucell_t)x); break;
		case CONSOLE_PRINT_HEX:			printf("$%4x ", (console_ucell_t)x); break;
		case CONSOLE_PRINT_STR:			/* fall through... */
		case CONSOLE_PRINT_STR_P:		printf("%s ", (const char*)x); break;
		default:						/* ignore */; break;
	}
}
#endif

#include "console-cmds-builtin.h"

// Execute a single command from a string
static uint8_t execute(char* cmd) { 
	static const console_recogniser RECOGNISERS[] PROGMEM = {
		/* The number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
			can have potentially any hash value so could look like commands. */
		console_r_number_decimal,		
		console_r_number_hex,			
		console_r_string,				
		console_r_hex_string,
		console_cmds_user,			// User defined function. 
		console_cmds_builtin,		// From "console-cmds-builtin.h"
	};

	// Establish a point where raise will go to when raise() is called.
	console_rc_t command_rc = setjmp(g_console_ctx.jmpbuf); // When called in normal execution it returns zero. 
	if (CONSOLE_RC_OK != command_rc)
		return command_rc;
	
	// try all recognisers in turn until one works. 
	for (uint8_t i = 0; i < ELEMENT_COUNT(RECOGNISERS); i += 1) {
		const console_recogniser recog = (console_recogniser)pgm_read_word(&RECOGNISERS[i]);
		if (recog(cmd))									// Call recogniser function.
			return CONSOLE_RC_OK;	 									/* Recogniser succeeded. */
	}
	return CONSOLE_RC_ERROR_UNKNOWN_COMMAND;
}

static bool is_whitespace(char c) {
	return (' ' == c) || ('\t' == c);
}

// External functions.

void consoleInit() {
	clear_stack();
}

console_rc_t consoleProcess(char* str) {
	// Iterate over input, breaking into words.
	while (1) {
		while (is_whitespace(*str)) 									// Advance past leading spaces.
			str += 1;
		
		if ('\0' == *str)												// Stop at end.
			break;

		// Record start & advance until we see a space.
		char* cmd = str;
		while ((!is_whitespace(*str)) && ('\0' != *str))
			str += 1;
		
		/* Terminate this command and execute it. We crash out if execute() flags an error. */
		if (cmd == str) 				// If there is no command to execute then we are done.
			break;
		
		const bool at_end = ('\0' == *str);		// Record if there was already a nul at the end of this string.
		if (!at_end) {
			*str = '\0';							// Terminate white space delimited command in strut buffer.
			str += 1;								// Advance to next char.
		}
		
		/* Execute parsed command and exit on any abort, so that we do not exwcute any more commands.
			Note that no error is returned for any negative eror codes, which is used to implement comments with the 
			CONSOLE_RC_SIGNAL_IGNORE_TO_EOL code. */
		const console_rc_t command_rc = execute(cmd);
		if (CONSOLE_RC_OK != command_rc) 
			return (command_rc < CONSOLE_RC_OK) ? CONSOLE_RC_OK : command_rc;
		
		// If last command break out of loop.
		if (at_end)
			break;
	}

	return CONSOLE_RC_OK;
}

// Print description of error code. 
const char* consoleGetErrorDescription(console_rc_t err) {
	switch(err) {
		case CONSOLE_RC_ERROR_NUMBER_OVERFLOW: return PSTR("number overflow");
		case CONSOLE_RC_ERROR_DSTACK_UNDERFLOW: return PSTR("stack underflow");
		case CONSOLE_RC_ERROR_DSTACK_OVERFLOW: return PSTR("stack overflow");
		case CONSOLE_RC_ERROR_UNKNOWN_COMMAND: return PSTR("unknown command");
		case CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW: return PSTR("input buffer overflow");
		default: return PSTR("");
	}
}

// Input functions.
void consoleAcceptClear() { 
	f_accept_context.inbidx = 0; 
}

console_rc_t consoleAccept(char c) {
	bool overflow = (f_accept_context.inbidx >= sizeof(f_accept_context.inbuf));
	
	if (CONSOLE_INPUT_NEWLINE_CHAR == c) {
		f_accept_context.inbuf[f_accept_context.inbidx] = '\0';
		consoleAcceptClear();
		return overflow ? CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW : CONSOLE_RC_OK;
	}
	else {	
		if ((c >= ' ') && (c < (char)0x7f)) {	 // Is is printable?
			if (!overflow)
				f_accept_context.inbuf[f_accept_context.inbidx++] = c;
		}
		return CONSOLE_RC_ACCEPT_PENDING;
	}
}
char* consoleAcceptBuffer() { return f_accept_context.inbuf; }

// Test functions 
uint8_t consoleStackDepth() { return u_depth(); }
console_cell_t consoleStackPick(uint8_t i) { return u_pick(i); }
void consoleReset() { clear_stack(); }
//uint8_t consoleAcceptBufferLen() { return f_accept_context.inbidx; }