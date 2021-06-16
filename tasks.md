* Tasks

Date		| Description																						
*20210605*	| Fix up copying source files to example dirs.						| Atmel Studio project uses include path setting to find headers. <br/>
																					Source file console.cpp added as reference.<br/>
																					Still need to fixup Arduino projects.
																					
*20210610*	| Change to class interface.
*20210612*	| Stop using setbuf(), just have a return for error codes.
*20210612*	| Add service() method that implements a complete console <br/>
				with minimal effort.
*20210612*	| Add setPromptStr() & setOutputLeaderStr() methods.
*20210613*	| Think: how to implement a text interface where input goes<br/>
				to a device, but can be directed to the console.<br/>
				Could require console stuff to be preceeded by a<br/>
				specific string, eg `$$$`, or have a command mode like modems.

*20210613*	| Get files checked in properly, install Tortoise??