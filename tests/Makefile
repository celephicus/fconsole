CC := gcc

DEFINES := -DCONSOLE_USE_LOCAL_CONFIG
CFLAGS := -g -O3 -Wall -Wpedantic -Wextra -Wconversion -Wduplicated-cond -Wlogical-op -Wmissing-declarations \
			-Wpadded -Wshadow -Wstrict-prototypes -Wswitch-default -Wwrite-strings
GCOV_CFLAGS = --coverage -O0
GCOV_LDFLAGS = -lgcov --coverage 

SRCDIR = ../src
INCLUDES := -I. -I$(SRCDIR)
HEADERS = console-config-local.h $(SRCDIR)/console.h
TARGET := tests

# Run script to preprocess all source files to generate definitions of console commands. 
$(shell ./prebuild.sh)

.PHONY: clean all 

all: $(TARGET)

$(TARGET): main.o console.o
	$(CC) $(GCOV_LDFLAGS) -o $@ main.o console.o

#	$(SRCDIR)/console-mk.py $(SRCDIR)/console.c

main.o: main.c $(HEADERS)  
	$(CC) $(CFLAGS) $(GCOV_CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@	

console.o: $(SRCDIR)/console.c $(HEADERS) 
	$(CC) $(CFLAGS) $(GCOV_CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@	

clean:
	-rm -f *.o $(TARGET)

