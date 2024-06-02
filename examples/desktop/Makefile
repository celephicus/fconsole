CC := g++

DEFINES := -DCONSOLE_USE_LOCAL_CONFIG
CFLAGS := -g -O3 -Wall -Wpedantic -Wextra -Wconversion -Wduplicated-cond -Wlogical-op -Wmissing-declarations \
			-Wpadded -Wshadow -Wstrict-prototypes -Wswitch-default -Wwrite-strings

SRCDIR = ../../src
INCLUDES := -I. -I$(SRCDIR)
HEADERS = console-config-local.h $(SRCDIR)/console.h
TARGET := desktop

# Run script to preprocess all source files to generate definitions of console commands. 
$(shell ./prebuild.sh)

.PHONY: clean all 

all: $(TARGET)

$(TARGET): main.o console.o
	$(CC) -o $@ main.o console.o

#	$(SRCDIR)/console-mk.py $(SRCDIR)/console.cpp

main.o: main.cpp $(HEADERS)  
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@	

console.o: $(SRCDIR)/console.cpp $(HEADERS) 
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) -c $< -o $@	

clean:
	-rm -f *.o $(TARGET)

