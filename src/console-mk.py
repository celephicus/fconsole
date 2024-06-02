#! /usr/bin/python3

# Open an input file and write it back.
# 	case /** . **/ 0xb58b: console_print_signed_decimal(); break;
# Lines that match `/** <PRINTABLE-CHARS> **/ 0x<hex-chars>:' have the hex chars replaced with a hash of the printable chars.
import re, sys, os, glob

PROGNAME = os.path.splitext(os.path.basename(sys.argv[0]))[0]

def subber_hash(m):
    HASH_START, HASH_MULT = 5381, 33 # No basis for these numbers, they just seem to work.
    h = HASH_START;
    w = m.group(1).upper()
    for c in w:
        h = ((h * HASH_MULT) & 0xffff) ^ ord(c)

    return '/** %s **/ 0X%04X' % (w, h)

for infile in sum([glob.glob(_, recursive=True) for _ in sys.argv[1:]], []):
	text = open(infile, 'rt').read()
	existing = text

	text = re.sub(r'''
	  /\*\*\s*		# `/** <spaces>'
	  (\S+)			# Command name, any non-whitespace characters.
	  \s*\*\*/		# `<spaces> **/'
	  \s*			# More spaces.
	  (0[x])?([0-9a-z]*)	# Hex number withleading `0x'.
	''', subber_hash, text, flags=re.I|re.X)

	print(f"{PROGNAME}: Processing {infile}: ", file=sys.stderr, end='')
	if text != existing:
		print("updated.", file=sys.stderr)
		open(infile, 'wt').write(text)
	else:
		print("skipped as unchanged.", file=sys.stderr)

