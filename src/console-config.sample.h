#ifndef CONSOLE_CONFIG_H__
#define CONSOLE_CONFIG_H__

/* This is the configuration file for fconsole. It is a good start for your own version, but should work for AVR &
	32/64 bit Linux targets. */

// We must have macros PSTR that places a const string into const memory, and READ_FUNC_PTR that deferences a pointer to a function in Flash.
#if defined(AVR)
 #include <Arduino.h>
 #include <avr/pgmspace.h>	
 #define CONSOLE_READ_PTR(x_) pgm_read_ptr(x_)		
 #define CONSOLE_READ_BYTE(x_) pgm_read_byte(x_)		
 #define CONSOLE_PROGMEM PROGMEM	
 #define CONSOLE_PSTR(s_) PSTR(s_)	
#elif defined(ESP32 )
 #include <Arduino.h>
 #include <pgmspace.h>	
 #define CONSOLE_READ_PTR(x_) pgm_read_ptr(x_)		
 #define CONSOLE_READ_BYTE(x_) pgm_read_byte(x_)		
 #define CONSOLE_PROGMEM PROGMEM	
 #define CONSOLE_PSTR(s_) PSTR(s_)	
#else
 #define CONSOLE_READ_PTR(x_) (*(x_))					
 #define CONSOLE_READ_BYTE(x_) (*(x_))
 #define CONSOLE_PROGMEM /* empty */	
 #define CONSOLE_PSTR(s_) s_
#endif

/* Our little language only works with one integral type, which is usally it is the natural int for the part. 
	Define to allow different types for this type. 
	Types console_int_t must be a signed type, console_uint_t must be unsigned, and they must both be the same size. 
	Also note that the type must be able to represent a pointer. */
#include <stdint.h>
#if   defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 2)
 typedef int16_t	console_int_t;
 typedef uint16_t  console_uint_t;
#elif defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 4)
 typedef int32_t	console_int_t;
 typedef uint32_t  console_uint_t;
#elif defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
 typedef int64_t	console_int_t;
 typedef uint64_t  console_uint_t;
#else
 typedef int      console_int_t;
 typedef unsigned console_uint_t;
#endif

/* For efficient implementation we have a small signed & unsigned type. Usually [u]int8_t will work fine. */
typedef uint8_t console_small_uint_t;
typedef int8_t  console_small_int_t;

// For unit test we want printf format modifier for integer types.
#include <limits.h>
#if INTPTR_MAX > INT_MAX
 #define CONSOLE_PRINTF_FMT_MOD "l"
#else
 #define CONSOLE_PRINTF_FMT_MOD ""
#endif

// Stack size, we don't need much.
#define CONSOLE_DATA_STACK_SIZE (8)

// Input buffer size
#define CONSOLE_INPUT_BUFFER_SIZE 40

// Character to signal EOL for input string.
#define CONSOLE_INPUT_NEWLINE_CHAR '\r'

// String for output newline.
#define CONSOLE_OUTPUT_NEWLINE_STR "\n"

// We want some help included. 
#define CONSOLE_WANT_HELP

// Define a function helper for consolePrint() that writes to a stream.
#define CONSOLE_WANT_PRINT_FUNC

#endif // CONSOLE_CONFIG_H__