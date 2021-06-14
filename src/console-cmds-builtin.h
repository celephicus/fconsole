
// Essential commands that will always be required
static bool console_cmds_builtin(char* cmd) {
	switch (hash(cmd)) {
		case /** . **/ 0xb58b: console_print_signed_decimal(); break;		// Pop and print in signed decimal.
		case /** U. **/ 0x73de: console_print_unsigned_decimal(); break;	// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0x658f: console_print_hex(); break;					// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0x66c9: console_print_string(); break; 				// Pop and print string.
		case /** DEPTH **/ 0xb508: u_push(u_depth()); break;				// Push stack depth.
		case /** CLEAR **/ 0x9f9c: clear_stack(); break;					// Clear stack so that it has zero depth.
		case /** DROP **/ 0x5c2c: u_pop(); break;							// Remove top item from stack.
		case /** HASH **/ 0x90b7: { console_cell_t* tos = u_tos(); *tos = hash((const char*)*tos); } break; // Pop string and push hash value.
		default: return false;
	}
	return true;
}
