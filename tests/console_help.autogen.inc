// This file is autogenerated -- do not edit.

static const char cmd_help_B58B[] PROGMEM = PSTR(". (d - ) Pop and print as signed decimal.");
static const char cmd_help_73DE[] PROGMEM = PSTR("U. (u - ) Pop and print as unsigned decimal, with leading `+'.");
static const char cmd_help_658F[] PROGMEM = PSTR("$. (u - ) Pop and print as 4 hex digits with leading `$'.");
static const char cmd_help_66C9[] PROGMEM = PSTR(".\" (s - ) Pop and print string.");
static const char cmd_help_B508[] PROGMEM = PSTR("DEPTH ( - u) Push stack depth.");
static const char cmd_help_9F9C[] PROGMEM = PSTR("CLEAR ( ... - <empty>) Remove all items from stack.");
static const char cmd_help_5C2C[] PROGMEM = PSTR("DROP (x - ) Remove top item from stack.");
static const char cmd_help_90B7[] PROGMEM = PSTR("HASH (s - u) Pop string and push hash value.");
static const char cmd_help_B58E[] PROGMEM = PSTR("+ ( x1 x2 - x3) Add: x3 = x1 + x2.");
static const char cmd_help_B588[] PROGMEM = PSTR("- ( x1 x2 - x3) Subtract: x3 = x1 - x2.");
static const char cmd_help_B58A[] PROGMEM = PSTR("/ ( d1 d2 - d3) Signed dvide: d3 = d1 / d2.");
static const char cmd_help_73DF[] PROGMEM = PSTR("U/ ( u1 u2 - u3) Signed dvide: u3 = u1 / u2.");
static const char cmd_help_7A79[] PROGMEM = PSTR("NEGATE ( d1 - d2) Negate signed value: d2 = -d1.");
static const char cmd_help_B586[] PROGMEM = PSTR("# ( - ) Comment, rest of input ignored.");
static const char cmd_help_4069[] PROGMEM = PSTR("RAISE ( i - ) Raise value as exception.");
static const char cmd_help_C745[] PROGMEM = PSTR("EXIT ( - ?) Exit console.");
static const char cmd_help_13B4[] PROGMEM = PSTR("PICK (u - x) Copy stack item by index.");
static const char cmd_help_398B[] PROGMEM = PSTR("OVER (x1 x2 - x1 x2 x1) Copy second stack item.");
static const char cmd_help_B0B4[] PROGMEM = PSTR("??HELP ( - ) Print (wordy) help for all commands.");
static const char cmd_help_74CB[] PROGMEM = PSTR("?HELP ( - ) Print list of all commands.");
static const char cmd_help_7D54[] PROGMEM = PSTR("HELP (s - ) Search for help on given command.");

static const char* const help_cmds[] PROGMEM = {
    cmd_help_B58B,
    cmd_help_73DE,
    cmd_help_658F,
    cmd_help_66C9,
    cmd_help_B508,
    cmd_help_9F9C,
    cmd_help_5C2C,
    cmd_help_90B7,
    cmd_help_B58E,
    cmd_help_B588,
    cmd_help_B58A,
    cmd_help_73DF,
    cmd_help_7A79,
    cmd_help_B586,
    cmd_help_4069,
    cmd_help_C745,
    cmd_help_13B4,
    cmd_help_398B,
    cmd_help_B0B4,
    cmd_help_74CB,
    cmd_help_7D54,
};

static const uint16_t help_hashes[] PROGMEM = {
    0xB58B,
    0x73DE,
    0x658F,
    0x66C9,
    0xB508,
    0x9F9C,
    0x5C2C,
    0x90B7,
    0xB58E,
    0xB588,
    0xB58A,
    0x73DF,
    0x7A79,
    0xB586,
    0x4069,
    0xC745,
    0x13B4,
    0x398B,
    0xB0B4,
    0x74CB,
    0x7D54,
};
