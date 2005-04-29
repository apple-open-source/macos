#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# 'info sharedlibext' returns '.dll' on Windows and '.so' on most Unix systems

load libsquare[info sharedlibext]

# Create a sound object

snack::sound s

# Set its length to 10000 samples

s length 10000

# Apply the command defined in the square package

s square

pack [button .b -text Play -command {s play}]
