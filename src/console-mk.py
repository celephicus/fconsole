# Open an input file and write it back.
# 	case /** . **/ 0xb58b: console_print_signed_decimal(); break;										\
# Lines that match `/** <printable-chars> **/ 0x<hex-chars>:' have the hec hars replaced with a hash of the printble chars.
import re, sys, argparse, os

infile = sys.argv[1]
text = open(infile, 'rt').read()
existing = text

def subber_macros(m):
    HASH_START, HASH_MULT = 5381, 33 # No basis for these numbers, they just seem to work.
    h = HASH_START;
    w = m.group(1).upper()
    for c in w:
        h = ((h * HASH_MULT) & 0xffff) ^ ord(c)
    
    return '/** %s **/ 0x%04x' % (w, h)  
text = re.sub(r'/\*\*\s*(\S+)\s*\*\*/\s*(0[x])?([0-9a-z]*)', subber_macros, text, flags=re.I)
    
if text != existing:    
    print("Writing file %s." % infile, file=sys.stderr)
    open(infile, 'wt').write(text)
else:
    print("Skipped writing file %s as unchanged." % infile, file=sys.stderr)
    