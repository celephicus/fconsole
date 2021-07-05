
// Essential commands that will always be required
static bool console_cmds_builtin(char* cmd) {
	switch (console_hash(cmd)) {
		case /** . **/ 0xb58b: consolePrint(CONSOLE_PRINT_SIGNED, u_pop()); break;		// Pop and print in signed decimal.
		case /** U. **/ 0x73de: consolePrint(CONSOLE_PRINT_UNSIGNED, u_pop()); break;	// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0x658f: consolePrint(CONSOLE_PRINT_HEX, u_pop()); break;		// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0x66c9: consolePrint(CONSOLE_PRINT_STR, u_pop()); break; 		// Pop and print string.
		case /** DEPTH **/ 0xb508: u_push(u_depth()); break;							// Push stack depth.
		case /** CLEAR **/ 0x9f9c: clear_stack(); break;								// Clear stack so that it has zero depth.
		case /** DROP **/ 0x5c2c: u_pop(); break;										// Remove top item from stack.
		case /** HASH **/ 0x90b7: { *u_tos() = console_hash((const char*)*u_tos()); } break;	// Pop string and push hash value.
		default: return false;
	}
	return true;
}
