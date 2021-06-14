
// User commands, these are really examples...
static bool console_cmds_user(char* cmd) {
	switch (hash(cmd)) {
		case /** + **/ 0xb58e: binop(+); break;
		case /** - **/ 0xb588: binop(-); break;
		case /** NEGATE **/ 0x7a79: unop(-); break;
		case /** # **/ 0xb586: raise(CONSOLE_ERROR_IGNORE_TO_EOL); break;
		case /** LED **/ 0xdc88: digitalWrite(13, !!u_pop()); break;
		default: return false;
	}
	return true;
}
