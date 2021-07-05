
// Essential commands that will always be required
static bool console_cmds_builtin(char* cmd) {
	switch (console_hash(cmd)) {
		case /** . **/ 0xb58b: consolePrint(CONSOLE_PRINT_SIGNED, console_u_pop()); break;		// Pop and print in signed decimal.
		case /** U. **/ 0x73de: consolePrint(CONSOLE_PRINT_UNSIGNED, console_u_pop()); break;	// Pop and print in unsigned decimal, with leading `+'.
		case /** $. **/ 0x658f: consolePrint(CONSOLE_PRINT_HEX, console_u_pop()); break;		// Pop and print as 4 hex digits decimal with leading `$'.
		case /** ." **/ 0x66c9: consolePrint(CONSOLE_PRINT_STR, console_u_pop()); break; 		// Pop and print string.
		case /** DEPTH **/ 0xb508: console_u_push(console_u_depth()); break;							// Push stack depth.
		case /** CLEAR **/ 0x9f9c: console_u_clear(); break;								// Clear stack so that it has zero depth.
		case /** DROP **/ 0x5c2c: console_u_pop(); break;										// Remove top item from stack.
		case /** HASH **/ 0x90b7: { *console_u_tos() = console_hash((const char*)*console_u_tos()); } break;	// Pop string and push hash value.
		default: return false;
	}
	return true;
}
