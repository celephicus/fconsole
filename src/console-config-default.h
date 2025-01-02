#ifndef CONSOLE_CONFIG_DEFAULT_H__
#define CONSOLE_CONFIG_DEFAULT_H__

/* This is the default configuration file. It may be used as a template for a local configuration file `console-locals.h' 
	which is included if `CONSOLE_USE_LOCALS' is defined. */

#include <Arduino.h>

/* Our little language only works with one integral type, which is sually it is the natural int for the part. For Arduino this is 16 bits.
	Define to allow different types for this type. 
	Types console_int_t must be a signed type, console_uint_t must be unsigned, and they must both be the same size. 
	Also note that the type must be able to represent a pointer. */
#include <stdint.h>
typedef int16_t  console_int_t;
typedef uint16_t console_uint_t;

/* For efficient implementation we have a small signed & unsigned type. Usually [u]int8_t will work fine. */
typedef uint8_t console_small_uint_t;
typedef int8_t  console_small_int_t;

// For unit test we want printf format for signed console type.
#define CONSOLE_FORMAT_INT "d"

// Stack size, we don't need much.
#define CONSOLE_DATA_STACK_SIZE (8)

// Input buffer size
#define CONSOLE_INPUT_BUFFER_SIZE 40

// Character to signal EOL for input string.
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

// We want some help included. 
#define CONSOLE_WANT_HELP

#endif // CONSOLE_CONFIG_DEFAULT_H__
