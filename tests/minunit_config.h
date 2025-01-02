// Config header for minunit.

// Static buffer to build messages in.
#define MINUNIT_MESSAGE_BUFFER_LEN 200

// Macro for delaring a constant string. Only required for gcc-avr with its PSTR() macro.
#ifndef MINUNIT_DECL_STR
#define MINUNIT_DECL_STR(s_) (s_)
#endif

