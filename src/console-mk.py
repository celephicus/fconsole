#! /usr/bin/python3

# Open an input file and write it back.
# 	case /** . **/ 0xb58b: console_print_signed_decimal(); break;
# Lines that match `/** <PRINTABLE-CHARS> **/ 0x<hex-chars>:' have the hex chars replaced with a hash of the printable chars.
import re, sys, os, glob

PROGNAME = os.path.splitext(os.path.basename(sys.argv[0]))[0]

HELP_FN = 'console_help.autogen.inc'

cmds = {}
def subber_hash(m):
	HASH_START, HASH_MULT = 5381, 33 # No basis for these numbers, they just seem to work.
	h = HASH_START;
	cmd = m.group(1).upper()
	help = m.group(2).strip()
	for c in cmd:
		h = ((h * HASH_MULT) & 0xffff) ^ ord(c)

	if cmd in cmds:
		error(f"duplicate hash for `{cmd}")   
	cmds[h] = (cmd, help)
	return f"/** {cmd} {help} **/ 0X{h:04X}"

output_dir = None
for infile in sum([glob.glob(_, recursive=True) for _ in sys.argv[1:]], []):
	text = open(infile, 'rt').read()
	if output_dir is None:
		output_dir = os.path.dirname(infile)
	existing = text

	text = re.sub(r'''
	  /\*\*\s*		# `/** <spaces>'
	  (\S+)			# Command name, any non-whitespace characters.
	  (.*)			# Help text.
	  \*\*/			# `**/'
	  \s*			# More spaces.
	  (0[x])?([0-9a-z]*)	# Hex number withleading `0x'.
	''', subber_hash, text, flags=re.I|re.X)

	print(f"{PROGNAME}: Processing {infile}: ", file=sys.stderr, end='')
	if text != existing:
		print("updated.", file=sys.stderr)
		open(infile, 'wt').write(text)
	else:
		print("skipped as unchanged.", file=sys.stderr)

def ss(s): return s.replace('"', '\\"')
help_decl_cmd_text = '\n'.join([f'static const char cmd_help_{h:04X}[] = PSTR("{ss(c[0] + " " + c[1])}");' for h, c in cmds.items()])
help_decl_cmds = '\n'.join([f'    cmd_help_{h:04X},' for h in cmds])
help_decl_hashes = '\n'.join([f'    0x{h:04X},' for h in cmds])
				
if output_dir is not None:
	with open(os.path.join(output_dir, HELP_FN), 'wt') as f:
		f.write(f"""\
// This file is autogenerated -- do not edit.

{help_decl_cmd_text}

static const char* const help_cmds[] = {{
{help_decl_cmds}
}};

static const uint16_t help_hashes[] = {{
{help_decl_hashes}
}};

""")
