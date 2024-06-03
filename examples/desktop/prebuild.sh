#!/bin/bash

# Make relative paths work when called from another dir. 
scriptdir="$(dirname "$0")"
cd "$scriptdir"

../../src/console-mk.py main.c ../../src/console.c
