#include <Arduino.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "console.h"

// How many elements in an array?
#define ELEMENT_COUNT(x_) (sizeof(x_) / sizeof((x_)[0]))

// Predicates for push & pop.
#define canPop(n_) (f_ctx.sp < (STACKBASE - (n_) + 1))
#define canPush(n_) (f_ctx.sp >= &f_ctx.dstack[0 + (n_)])

// Error handling in commands.
#define verifyCanPop(n_)	do { if (!canPop(n_)) raise(CONSOLE_RC_ERROR_DSTACK_UNDERFLOW); } while (0)
#define verifyCanPush(n_)	do { if (!canPush(n_)) raise(CONSOLE_RC_ERROR_DSTACK_OVERFLOW); } while (0)


// Stack primitives.
console_cell_t Fconsole::stackbase() { return &_dstack[CONSOLE_DATA_STACK_SIZE]; }		// Stack fills from top down.
console_cell_t Fconsole::u_pick_unchecked(uint8_t i) 	{ return _sp[i]; }
console_cell_t* Fconsole::u_tos() 			{ verifyCanPop(1); return _sp }
console_cell_t* Fconsole::u_nos() 			{ verifyCanPop(2); return _sp + 1; }
console_cell_t Fconsole::u_depth() 			{ return (stackbase() - _sp); }
console_cell_t Fconsole::u_pop() 			{ verifyCanPop(1); return *_sp++; }
void u_push(console_cell_t x) 				{ verifyCanPush(1); *--_sp = x; }
void clear_stack()							{ _sp = stackbase(); }

// Call on error, thanks to the magic of longjmp() it will return to the last setjmp with the error code.
void raise(console_rc_t rc) {
	longjmp(f_ctx.jmpbuf, rc);
}

//
// Public methods.
//

Fconsole::Fconsole(Stream& stream) : _stream(stream) { }
	
void Fconsole::begin(Stream& stream) {
	reset();
}

void Fconsole::service() {}
	
const char* Fconsole::getErrorDescription(console_rc_t err) {
	switch(err) {
		case CONSOLE_RC_ERROR_NUMBER_OVERFLOW: return PSTR("number overflow");
		case CONSOLE_RC_ERROR_DSTACK_UNDERFLOW: return PSTR("stack underflow");
		case CONSOLE_RC_ERROR_DSTACK_OVERFLOW: return PSTR("stack overflow");
		case CONSOLE_RC_ERROR_UNKNOWN_COMMAND: return PSTR("unknown command");
		case CONSOLE_RC_ERROR_ACCEPT_BUFFER_OVERFLOW: return PSTR("input buffer overflow");
		default: return PSTR("");
	}
}
console_rc_t Fconsole::process(char* str);
void Fconsole::acceptClear() { 
	_inbidx = 0;

}
console_rc_t Fconsole::accept(char c);

char* Fconsole::acceptBuffer() { return _inbuf; }

uint8_t Fconsole::stackDepth() { return u_depth(); }
	
console_cell_t Fconsole::stackPick(uint8_t i) { 
	return u_pick_unchecked(i); 
}
	
void Fconsole::reset() {
	clear_stack();
	acceptClear();
}
	
//
// Protected methods.
//

// Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
// The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
// All characters are hashed even non-printable ones.
uint16_t Fconsole::hash(const char* str) {
	uint16_t h = Fconsole::HASH_START;
	char c;
	while ('\0' != (c = *str++)) {
		if ((c >= 'a') && (c <= 'z'))
		c -= 'a' - 'A';
		h = (h * Fconsole::HASH_MULT) ^ (uint16_t)c;
	}
	return h;
}


// Convert a single character in range [0-9a-zA-Z] to a number up to 25. A negative value is returned on error. 
int8_t Fconsole::convert_digit(char c) {
	if ((c >= '0') && (c <= '9')) 
		return (int8_t)c - '0';
	else if ((c >= 'a') && (c <= 'z')) 
		return (int8_t)c -'a' + 10;
	else if ((c >= 'A') && (c <= 'Z')) 
		return (int8_t)c -'A' + 10;
	else 
		return -1;
}

// Convert an unsigned number of any base up to 36. Return true on success.
bool Fconsole::convert_number(console_ucell_t* number, console_cell_t base, const char* str) {
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
			raise(Fconsole::RC_ERROR_NUMBER_OVERFLOW);
	}
	
	return true;		// If we get here then it must have worked. 
}  

/* Recogniser for signed/unsigned decimal number.
	The number format is as follows:
		An initial '-' flags the number as negative, the '-' character is illegal anywhere else in the word.
		An initial '+' flags the number as unsigned. In which case the range of the number is up to the maximum
			_unsigned_ value for the type before overflow is flagged.
	Return values are good, bad, overflow.
	*/
bool Fconsole::console_r_number_decimal(char* cmd) {
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
			raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		break;
	case '-':		/* Signed negative number. */
		if (result > ((console_ucell_t)CONSOLE_CELL_MIN))
			raise(CONSOLE_RC_ERROR_NUMBER_OVERFLOW);
		result = (console_ucell_t)-(console_cell_t)result;
		break;
	}

	// Success.
	u_push(result);
	return true;
}

/* Recogniser for hex numbers preceded by a '$'. Return values are good, bad, overflow. */
bool Fconsole::console_r_number_hex(char* cmd) {
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
bool Fconsole::console_r_string(char* cmd) {
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
					const uint8_t digit_1 = convert_digit(rp[0]);
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

// Output routines.
void Fconsole::console_print_string() 			{ _stream.print((const char*)u_pop()); _stream.print(' '); }
void Fconsole::console_print_signed_decimal() 	{ _stream.print(u_pop(), DEC); _stream.print(' '); }
void Fconsole::console_print_unsigned_decimal() { const console_ucell_t x = (console_ucell_t)u_pop(); _stream.print('+'); _stream.print(x, DEC); _stream.print(' '); }
void Fconsole::console_print_hex() 				{ // I miss printf()! This could be replaced with sprintf() but it's huge.
	char buf[sizeof(console_ucell_t)*2+1];
	char *b = buf + sizeof(buf);
	*--b = '\0';
	console_ucell_t x = u_pop();
	for (uint8_t i = 0; i < sizeof(console_ucell_t)*2; i += 1) {
		*--b = (x & 15) + '0';
		if (*b > '9')
			*b += 'A' - '9' -1;
		x >>= 4;
	}
	_stream.print('$'); 
	_stream.print(buf); 
	_stream.print(' '); 
}

/* Some helper macros for commands. */
#define binop(op_) 	{ const console_cell_t rhs = u_pop(); *u_tos() = *u_tos() op_ rhs; } 	// Implement a binary operator.
#define unop(op_)  	{ *tos = op_ *tos; }													// Implement a unary operator.

// #include "console-cmds-builtin.h"
// #include "console-cmds-user.h"

bool Fconsole::console_cmds_builtin(char* cmd) {
	switch (hash(cmd)) {
		case /** . **/ 0xb58b: console_print_signed_decimal(); break;		// Pop and print in signed decimal.
		case /** U. **/ 0x73de: console_print_unsigned_decimal(); break;	// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0x658f: console_print_hex(); break;					// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0x66c9: console_print_string(); break; 				// Pop and print string.
		case /** DEPTH **/ 0xb508: u_push(u_depth()); break;				// Push stack depth.
		case /** CLEAR **/ 0x9f9c: clear_stack(); break;					// Clear stack so that it has zero depth.
		case /** DROP **/ 0x5c2c: u_pop(); break;							// Remove top item from stack.
		case /** HASH **/ 0x90b7: { *u_tos() = hash((const char*)*u_tos()); } break; // Pop string and push hash value.
		default: return false;
	}
	return true;
}

// Execute a single command from a string
uint8_t Fconsole::execute(char* cmd) { 
	static const console_recogniser RECOGNISERS[] PROGMEM = {
		/* The number & string recognisers must be before any recognisers that lookup using a hash, as numbers & strings
			can have potentially any hash value so could look like commands. */
		console_r_number_decimal,		
		console_r_number_hex,			
		console_r_string,				
		console_cmds_builtin,		
		console_cmds_user,		
	};

	// Establish a point where raise will go to when raise() is called.
	console_rc_t command_rc = setjmp(f_ctx.jmpbuf); // When called in normal execution it returns zero. 
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
		
		// Execute parsed command and exit on any abort, but return no error for a comment.
		const console_rc_t command_rc = execute(cmd);
		if (CONSOLE_RC_OK != command_rc) 
			return (CONSOLE_RC_SIGNAL_IGNORE_TO_EOL == command_rc) ? CONSOLE_RC_OK : command_rc;
		
		// If last command break out of loop.
		if (at_end)
			break;
	}

	return CONSOLE_RC_OK;
}


// Input functions.

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
