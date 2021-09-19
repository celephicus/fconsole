#ifndef CONSOLE_LOCALS_DEFAULT_H__
#define CONSOLE_LOCALS_DEFAULT_H__

// Our little language only works with one integral type, called a "cell" like FORTH. Usually it is the natural int for the part. For Arduino this is 16 bits. 
// Define to allow different types for the 'cell'. CONSOLE_CELL_T must be a signed type, CONSOLE_UCELL_T must be unsigned, and they must both be the same size.
#include <stdint.h>
typedef int16_t  console_cell_t;
typedef uint16_t console_ucell_t;

// For unit test we want printf format for signed cell.
#define CONSOLE_FORMAT_CELL "d"

// Stack size, we don't need much.
#define CONSOLE_DATA_STACK_SIZE (8)

// Input buffer size
#define CONSOLE_INPUT_BUFFER_SIZE 40

// Character to signal EOL for input string.
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

#endif // CONSOLE_LOCALS_DEFAULT_H__
