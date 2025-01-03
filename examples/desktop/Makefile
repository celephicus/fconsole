CC := gcc

DEFINES := 

# Set of fairly pedantic warnings flags.
CFLAGS := -g -O3 -Wall -Wpedantic -Wextra -Wconversion -Wduplicated-cond -Wlogical-op -Wmissing-declarations \
			-Wpadded -Wshadow -Wstrict-prototypes -Wswitch-default -Wwrite-strings -Wundef -Werror

# Where?
SRCDIR = ../../src
INCLUDES := -I. -I$(SRCDIR)
TARGET := desktop

SRCS = console.c minunit.h main.c 

# Run script to preprocess all source files to generate definitions of console commands. 
$(shell ./prebuild.sh)

.PHONY: clean all 
all: $(TARGET)

clean:
	-rm -f *.o $(TARGET)

# Source search dirs.
vpath %.c $(SRCDIR)
vpath %.h $(SRCDIR)

# Build executable.
$(TARGET): main.o console.o 
	$(CC) -o $@ $^

# Header dependancies.
main.o: console-config.h console.h 
console.o: console-config.h console.h

# One rule for all "C" source files.
%.o: %.c 
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@	
