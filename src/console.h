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

#define CONSOLE_INPUT_BUFFER_SIZE 40
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

class Fconsole {
public:
	// Construct with a stream for IO. 
	Fconsole(Stream& stream);
		
	// Initialise 
	void begin();

	// Function provided as a convenience, it reads characters from the stream until a CONSOLE_INPUT_NEWLINE_CHAR is read, then processes the line, writing the result to the stream,
	// with possible error messages. If you want an easy life then just use this. 
	void service();
	
	// Or you can use the methods below, which give you a lot more control...
	
	// Define possible error codes. The convention is that positive codes are actual errors, zero is OK, and negative values are more like status codes that do not indicate an error.
	enum {
		RC_OK =								0,	// Returned by consoleProcess() for no errors and by consoleAccept() if a newline was read with no overflow.
	
		// Errors: something has gone wrong.
		RC_ERROR_NUMBER_OVERFLOW =			1,	// Returned by consoleProcess() (via convert_number()) if a number overflowed it's allowed bounds,
		RC_ERROR_DSTACK_UNDERFLOW =			2,	// Stack underflowed (attempt to pop or examine too many items).
		RC_ERROR_DSTACK_OVERFLOW =			3,	// Stack overflowed (attempt to push too many items).
		RC_ERROR_UNKNOWN_COMMAND =			4,	// A command or value was not recognised.
		RC_ERROR_ACCEPT_BUFFER_OVERFLOW =	5,	// Accept input buffer has been sent more characters than it can hold. Only returned by consoleAccept(). 
	
		// Status
		RC_SIGNAL_IGNORE_TO_EOL = -1,			// Internal signal used to implement comments.
		RC_ACCEPT_PENDING = -2,					// Only returned by consoleAccept() to signal that it is still accepting characters.
		};

	// Type used for status codes from various public & protected methods.
	typedef int8_t console_rc_t;

	// Return a short description of the error as a pointer to a PROGMEM string. 
	const char* getErrorDescription(console_rc_t err);

	// Evaluate a line of string input.
	console_rc_t process(char* str);

	// Input functions, not essential but helpful.
	void acceptClear();

	/* Read chars into a buffer, returning CONSOLE_ERROR_ACCEPT_PENDING. Only CONSOLE_INPUT_BUFFER_SIZE chars are stored.
		If the character CONSOLE_INPUT_NEWLINE_CHAR is seen, then return CONSOLE_ERROR_OK if no overflow, else CONSOLE_ERROR_INPUT_OVERFLOW.
		In either case the buffer is nul terminated, but not all chars will have been stored on overflow. 
	*/
	console_rc_t accept(char c);
	char* acceptBuffer();

	// These functions are for unit tests and probably should not be used for real code.
	uint8_t stackDepth();
	console_cell_t stackPick(uint8_t i);
	
	// Reset the state of the interpreter and acceptor. Exposed for test purposes, not really required for normal usage. 
	void reset();

protected:
	// Stack primitives.
	console_cell_t stackbase();
	console_cell_t u_depth();
	void clear_stack();

	// Return nth item from top of stack. Index not checked so possible to go beyond the stack.
	console_cell_t u_pick_unchecked(uint8_t i);
	
	console_cell_t* u_tos();
	console_cell_t u_pop();
	void u_push(console_cell_t x);
	
	// Raise an "exception".
	void raise(console_rc_t rc);
	
	// Hash function as we store command names as a 16 bit hash. Lower case letters are converted to upper case.
	// The values came from Wikipedia and seem to work well, in that collisions between the hash values of different commands are very rare.
	// All characters are hashed even non-printable ones.
	enum { HASH_START = 5381, HASH_MULT =33, };
	uint16_t hash(const char* str);

	// Convert a single character in range [0-9a-zA-Z] to a number up to 25. A negative value is returned on error. Class method as does not use member data.
	static int8_t convert_digit(char c);	

	// Convert an unsigned number of any base up to 36. Return true on success.
	bool convert_number(console_ucell_t* number, console_cell_t base, const char* str);

	// Recognisers are little parser functions that can turn a string into a value or values that are pushed onto the stack. They return false if they cannot
	//  parse the input string. If they do parse it, they might call error if they cannot push a value onto the stack.
	typedef bool (*console_recogniser)(char* cmd);

	bool console_r_number_decimal(char* cmd);
	bool console_r_number_hex(char* cmd);
	bool console_r_string(char* cmd);

	// Will need replacing with 
	bool console_r_string(char* cmd);

	// Output routines.
	void console_print_string();
	void console_print_signed_decimal();
	void console_print_unsigned_decimal();
	void console_print_hex();

	// Execute a single command from a string
	uint8_t execute(char* cmd);
	
private:
	console_cell_t _dstack[CONSOLE_DATA_STACK_SIZE];
	console_cell_t* _sp;
	Stream& _stream;				// An opened stream for text IO.
	jmp_buf _jmpbuf;				// How we do aborts.
	char _inbuf[CONSOLE_INPUT_BUFFER_SIZE + 1];
	uint8_t _inbidx;
}	
};

#endif // CONSOLE_H__

