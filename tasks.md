# Tasks

Date		| Description														| Fixed?
---			|---																| ---
*20210605*	| Fix up copying source files to example dirs.						| Atmel Studio project uses include path setting to find headers. Source file console.cpp added as reference. Still need to fixup Arduino projects. Suggest script to copy files locally. Fixed properly -- use arduino-cli to install lib. Easy.
*20210610*	| Change to class interface.										| Done.
*20210612*	| Stop using setbuf(), just have a return for error codes.			| Decided not to do, setjmp isn't very big in terms of code size and is a very elegant solution. '
*20210612*	| Add service() method that implements a complete console with minimal effort. | Done.
*20210612*	| Add setPromptStr() & setOutputLeaderStr() methods.				| Easy to do but not worth it.
*20210613*	| Think: how to implement a text interface where input goes to a device, but can be directed to the console. Could require console stuff to be preceeded by a specific string, eg `$$$`, or have a command mode like modems. | Still thinking, added to wiki. 
*20210613*	| Get files checked in properly, install Tortoise??					| Fixed, just learnt to use command line git.
