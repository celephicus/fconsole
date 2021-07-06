# Open an input file and write it back.
# 	case /** . **/ 0xb58b: console_print_signed_decimal(); break;										
# Lines that match `/** <PRINTABLE-CHARS> **/ 0x<hex-chars>:' have the hex chars replaced with a hash of the printable chars.
import re, sys, glob

def subber_hash(m):
    HASH_START, HASH_MULT = 5381, 33 # No basis for these numbers, they just seem to work.
    h = HASH_START;
    w = m.group(1).upper()
    for c in w:
        h = ((h * HASH_MULT) & 0xffff) ^ ord(c)
    
    return '/** %s **/ 0X%04X' % (w, h)  

for infile in glob.glob(sys.argv[1], recursive=True):
	text = open(infile, 'rt').read()
	existing = text

	text = re.sub(r'/\*\*\s*(\S+)\s*\*\*/\s*(0[x])?([0-9a-z]*)', subber_hash, text, flags=re.I)
		
	if text != existing:    
		print("Updated file %s." % infile, file=sys.stderr)
		open(infile, 'wt').write(text)
	else:
		print("Skipped file %s as unchanged." % infile, file=sys.stderr)
		