#include <Arduino.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "console.h"
#include "console-internals.h"

// Check integral types. If this is wrong so much will break in surprising ways.
#define STATIC_ASSERT(expr_) extern int error_static_assert_fail__[(expr_) ? 1 : -1] __attribute__((unused))

// Is an integer type signed, works for chars as well.
#define utilsIsTypeSigned(T_) (((T_)(-1)) < (T_)0)

// And check for compatibility of the two cell types.
STATIC_ASSERT(sizeof(console_cell_t) == sizeof(console_ucell_t));
STATIC_ASSERT(utilsIsTypeSigned(console_cell_t));
STATIC_ASSERT(!utilsIsTypeSigned(console_ucell_t));

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

// Error handling in commands.
void console_verify_can_pop(uint8_t n) { if (!console_can_pop(n)) console_raise(CONSOLE_RC_ERROR_DSTACK_UNDERFLOW); }
void console_verify_can_push(uint8_t n) { if (!console_can_push(n)) console_raise(CONSOLE_RC_ERROR_DSTACK_OVERFLOW); }

// Stack primitives.
console_cell_t console_u_pick(uint8_t i)	{ return g_console_ctx.sp[i]; }
console_cell_t& console_u_tos() 			{ console_verify_can_pop(1); return *g_console_ctx.sp; }
console_cell_t& console_u_nos() 			{ console_verify_can_pop(2); return *(g_console_ctx.sp + 1); }
console_cell_t console_u_depth() 			{ return (CONSOLE_STACKBASE - g_console_ctx.sp); } 
console_cell_t console_u_pop() 				{ console_verify_can_pop(1); return *(g_console_ctx.sp++); }
void console_u_push(console_cell_t x) 		{ console_verify_can_push(1); *--g_console_ctx.sp = x; }
void console_u_clear()						{ g_console_ctx.sp = CONSOLE_STACKBASE; }

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

// Recognisers
//

// Regogniser for signed/unsigned decimal numbers.
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
			console_raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		break;
	case '-':		/* Signed negative number. */
		if (result > ((console_ucell_t)CONSOLE_CELL_MIN))
			console_raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		result = (console_ucell_t)-(console_cell_t)result;
		break;
	}

	// Success.
	console_u_push(result);
	return true;
}

// Recogniser for hex numbers preceded by a '$'. 
bool console_r_number_hex(char* cmd) {
	if ('$' != *cmd) 
		return false;

	console_ucell_t result;
	if (!convert_number(&result, 16, &cmd[1]))
		return false;
	
	// Success.
	console_u_push(result);
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
	console_u_push((console_cell_t)&cmd[0]);   	// Push address we started writing at.
	return true;
}

// Hex string with a leading '&', then n pairs of hex digits, pushes address of length of string, then binary data.
// So `&1aff01' will push a pointer to memory 03 1a ff 01.
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
	*len = out_ptr - len - 1; 		// Store length, looks odd, using len as a pointer and a value.
	console_u_push((console_cell_t)len);	// Push _address_.
	return true;
}

// Essential commands that will always be required
bool console_cmds_builtin(char* cmd) {
	switch (console_hash(cmd)) {
		case /** . **/ 0XB58B: consolePrint(CONSOLE_PRINT_SIGNED, console_u_pop()); break;		// Pop and print in signed decimal.
		case /** U. **/ 0X73DE: consolePrint(CONSOLE_PRINT_UNSIGNED, console_u_pop()); break;	// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0X658F: consolePrint(CONSOLE_PRINT_HEX, console_u_pop()); break;		// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0X66C9: consolePrint(CONSOLE_PRINT_STR, console_u_pop()); break; 		// Pop and print string.
		case /** DEPTH **/ 0XB508: console_u_push(console_u_depth()); break;					// Push stack depth.
		case /** CLEAR **/ 0X9F9C: console_u_clear(); break;									// Clear stack so that it has zero depth.
		case /** DROP **/ 0X5C2C: console_u_pop(); break;										// Remove top item from stack.
		case /** HASH **/ 0X90B7: { console_u_tos() = console_hash((const char*)console_u_tos()); } break;	// Pop string and push hash value.
		default: return false;
	}
	return true;
}

// Example output routines.
#if 0
void consolePrint(uint8_t opt, console_cell_t x) {
	switch (opt & 0x7f) {
		case CONSOLE_PRINT_NEWLINE:		printf(CONSOLE_OUTPUT_NEWLINE_STR)); (void)x; return;
		default:						/* ignore */; return;
		case CONSOLE_PRINT_SIGNED:		printf("%d ", x); break;
		case CONSOLE_PRINT_UNSIGNED:	printf("+%u ", (console_ucell_t)x); break;
		case CONSOLE_PRINT_HEX:			printf("$%4x ", (console_ucell_t)x); break;
		case CONSOLE_PRINT_STR:			/* fall through... */
		case CONSOLE_PRINT_STR_P:		puts((const char*)x); break;
		case CONSOLE_PRINT_CHAR:		putc((char)x); break;
	}
	if (!(opt & CONSOLE_PRINT_NO_SEP))	putc(' ');			// Print a space unless instructed not to. 
}
#endif

// Execute a single command from a string
static uint8_t execute(char* cmd) { 
	// Establish a point where raise will go to when raise() is called.
	console_rc_t command_rc = setjmp(g_console_ctx.jmpbuf); // When called in normal execution it returns zero. 
	if (CONSOLE_RC_OK != command_rc)
		return command_rc;
	
	// Try all recognisers in turn until one works. 
	const console_recogniser_func* rp = g_console_ctx.recognisers;
	console_recogniser_func r;
	while (NULL != (r = (console_recogniser_func)pgm_read_word(rp++))) {
		if (r(cmd))											// Call recogniser function.
			return CONSOLE_RC_OK;	 						// Recogniser succeeded. 
	}
	return CONSOLE_RC_ERROR_UNKNOWN_COMMAND;
}

static bool is_whitespace(char c) {
	return (' ' == c) || ('\t' == c);
}

// External functions.
//

void consoleInit(const console_recogniser_func* r_list) {
	g_console_ctx.recognisers = r_list;
	console_u_clear();
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
		return CONSOLE_RC_STATUS_ACCEPT_PENDING;
	}
}
char* consoleAcceptBuffer() { return f_accept_context.inbuf; }

// Test functions 
uint8_t consoleStackDepth() { return console_u_depth(); }
console_cell_t consoleStackPick(uint8_t i) { return console_u_pick(i); }
void consoleReset() { console_u_clear(); }
