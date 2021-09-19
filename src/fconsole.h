#ifndef _FCONSOLE_H__
#define _FCONSOLE_H__

#include "console.h"

class _FConsole {
public:
	_FConsole() {};
	void begin(console_recogniser_func r_user, Stream& s);
	void prompt();
	void service();
	
	// Shim function to call function pointer in RAM from a static function whose address can be in PROGMEM.
	static bool r_cmds_user(char* cmd) { return (s_r_user) ? s_r_user(cmd) : false; }
	
public:		// All data public and static.
	static console_recogniser_func s_r_user;
	static Stream* s_stream;
};

// An example of the Highlander pattern. There can be only one. If there isn't, they have a scrap and one gets it's head cut off.
extern _FConsole FConsole;

#endif