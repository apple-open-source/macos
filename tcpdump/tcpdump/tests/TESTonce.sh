#!/bin/sh

mkdir -p NEW DIFF

if [ $# -ne 4 ]; then
    echo "usage: ${PROGNAME} name input output options"
fi

name="$1"
input="$2"
output="$3"
options="$4"

echo "$ tcpdump -n -t -r $input $options" >>verbose-outputs.txt

#use eval otherwise $option may contain double quotes '"'
eval tcpdump 2>>verbose-outputs.txt -n -t -r $input $options >NEW/$output
r=$?
# emulate the return values of the perl system function
if [ $r -lt 128 ]; then
    let "r = r << 8"
fi
if [ $r -ne 0 ]; then
    # this means tcpdump failed.
    printf "EXIT CODE %08x\n" $r >> NEW/$output
    r=0
fi

cat NEW/$output | diff $output - >DIFF/$output.diff
r=$?
# emulate the return values of the perl system function
if [ $r -lt 128 ]; then
    let "r = r << 8"
fi

if [ $r -eq 0 ]; then
    printf "    %-35s: passed\n" $name
    unlink "DIFF/$output.diff"
    exit 0
fi

printf "    %-35s: TEST FAILED" $name
printf "Failed test: $name\n\n" >> failure-outputs.txt

if [ -a "DIFF/$output.diff" ]; then
    cat DIFF/$output.diff >> failure-outputs.txt
fi

if [ $r -eq -1 ]; then
    printf " (failed to execute: tcpdump -n -t -r $input $options)\n"
    exit 30
fi

# this is not working right, $r == 0x8b00 when there is a core dump.
# clearly, we need some platform specific perl magic to take this apart, so look for "core"
# too.
# In particular, on Solaris 10 SPARC an alignment problem results in SIGILL,
# a core dump and $r set to 0x00008a00 ($? == 138 in the shell).
if [ $(( $r & 127 )) -ne 0 -o -a "core" ]; then
    if [  $(( $r & 128 )) -ne 0 ]; then
         with="with"
    else
        with="without"
    fi
    if [ -a "core" ]; then
         with="with"
    fi
    printf " (terminated with signal %u, %s coredump)\n" $(( $r & 127 )) $with
    if [ $(( $r & 128 )) -ne 0 ];then
        exit 10
    else
        exit 20
    fi
fi

printf "\n"
exit $(( $r >> 8 ))
