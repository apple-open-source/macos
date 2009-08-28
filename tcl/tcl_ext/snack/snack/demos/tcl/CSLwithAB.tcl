#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# This file shows how to handle CSL/NSP files with both A and B data chunks

package require -exact snack 2.2

set filename CSL_file_with_A_and_B_chunks.nsp

# Read in file using Snack's CSL/NSP decoder, which only gets channel A
# This is done in order to determine the number of samples in each channel

snack::sound s -load $filename

set end [expr [s length] - 1]

# Now read channel A by treating the file as raw data skipping initial
# headers and write it out in a file of its own

s read $filename -fileformat raw -skip 60 -end $end

s write channel_A.wav

# Read channel B by skipping channel A and initial headers and write it

s read $filename -fileformat raw -skip [expr 60 + $end * 2 + 8] -end $end

s write channel_B.wav

exit

