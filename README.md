# arduino-console
A minimal framework for adding a console to Arduino projects, allowing you to type simple commands and to print results. 

## Introduction

What I have often wanted for my projects is a simple console that would give me the ability to run simple commands, to perform configuration, or perhaps testing. Since I am old I remember the old days when any piece of equipment with any pretensions to usability would have a 25 way D connector on the rear panel that would allow you to plug in a dumb terminal, and after learning some arcane command language, _interact_ with the equipment. 

## Inspiration

I remember a FORTH system at a small engineering company I worked for at school. It ran their office, did accounts, word processing, and also prepared paper tapes(!) for the CNC machines. It was an elderly early 70's minicomputer, and had 32K of memory, but could support half a dozen terminals. I was fascinated by the language that was like no other I had ever seen, except perhaps the APL that my Dad used for his statistics work. The engineers who knew the system could adapt it to do all sorts of useful things, usually with only a few lines of code that looked more like line noise. Fast forward a few years, and I got to play with a bizarre British home computer the Jupiter Ace, similar to the Sinclair ZX81 I suppose but it had FORTH in ROM and FORTH "words" (what other languages call keywords) as shortcuts on the keys. You programmed it in FORTH.

Anyway I decided to use a FORTH inspired syntax for my little console. I should also give a nod to a chapter "3.3 A Command-Line Interpreter" in the book "Embedded Software - The Works" by Colin Walls, pub. 2006, that got me thinking about the idea. I have not used any of his code, beyond the _idea_ of a FORTH-inspared interface. I should also give a shout-out to Github user zev's _zforth_, a tiny full featured FORTH for some good ideas, like the use of setjmp() for implementing abort.

## Requirements
	
	- Simple printable character based interface suitable for use over a serial terminal.
	- No requirement for command editing, the user sends a line of text terminated with a return character, and the console code interprets it and prints a response.
	- Minimal memory requirements.
	- Portable to different architectures.
	- Very easy to add new commands to.
	- Minimal syntax.
	- Ability to enter parameters, numbers & strings, and possibly other tyoes of data if required. 
	
## Syntax

Most of these requireents have already been addressed by the user interface to FORTH, which I'll describe.

FORTH programs consist of a sequence of "words" (which I call commands as it is too easy to confuse with machine words) that are seperated with one or more whitespace characters. That's about it. So the commands can be seperated by scanning for whitespace. 

The basic FORTH datatype is the natural word size for the processor, similar to the `int` datatype in "C".

FORTH also keeps values on a stack, so they can be easily accessed by removing (popping) the top value, then the next, and so on. Mos FORT commands take their parameters from the stack, and push the results onto the stack. Again, it's as simple as that.

An example: `2 3 * .` is parsed into 4 tokens, which are executed in turn. The token `2` is parsed as a number so it gets pushed on the stack, as does the token `3`. The token `*` requires 2 values, which it takes from the top 2 values on the stack, multiplies them and pushes the result, which is '6'. The token `.` takes the top value from the stack and print it  as a number.

But the console is not intended as a calculator, it's real job is to allow access to the system. So as an example my LED desk lamp has a command `led-rgb` that takes 3 values from the stack and sets the RGB channels of a LED to these values. So the command `255 20 0 led-rgb` sets a slightly off-red. I've also got a command `led-hsv` that sets the colour in HSV values. These commands allows me to test out the lamp long before I had any user interface written.

## Console Syntax

Short answer, there isn't any. Commands and numbers are case insensitive, you can use any combination of case you like. Strings handle case _exactly_ as typed. 


## Number Formats

The word size of the console is 16 bits, so a number range of -32768 to 32767 inclusive for signed integers, 0 trhrough 65535 for unsigned integers. 

Signed integers are simply written out, with a leading '-' for negative numbers. Any numbers that are out of range cause an error. I have found this range checking very useful to catch silly mistakes.

Unsigned integers are written with a leading '+', and are similarly checked for range.

Hex numbers are written with a leading '$', eg `$12af`, they are also range checked.

Strings are written with a leading '"', but with the wrinkle that they cannot contain a space, as this seperates the string into two tokens. But a space can be entered as an escape sequence `\20`. Other escape sequences are `\\`, `\r` & `\n`, and any character can be entered with a backslash followed by exactly 2 hex digits. The string is terminated with a nul character so it can be used as a regular "C" string, and the address is placed on the stack.
Note that the string is actually stored in the input buffer, so the data is only valid for the remaining commands on the line being processed. The next line will probably corrupt it. So strings must be used in the same line that they are entered. 

# Builtin Commands

A few commands are standard in the console, these are described below:

- `.` - A single dot. Pops the value on top of the stack and prints it as a signed decimal, so in the range -32768 through 32767 inclusive.
- `U.` - Pops the value on top of the stack and prints it as an unsigned decimal with a leading '+', so in the range 0 through 65536 inclusive.
- `$.` - Pops the value on top of the stack and prints it as 4 hex digits with a leading '$'.
- `."` - Pops the value on top of the stack and prints it as a string.
- `CLEAR` - Discards all values on the stack. Useful if you have lost track of what you have entered so far.seful if you have lost track of what you have entered so far.
- `HASH` - Computes the 16 bit hash value of the string address on the stack and pushes it onto the stack. Useful for adding new commands.

