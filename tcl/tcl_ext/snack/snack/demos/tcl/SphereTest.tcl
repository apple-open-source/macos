#!/bin/sh
# the next line restarts using wish \
exec tclsh8.4 "$0" "$@"

package require -exact snack 2.2
package require snacksphere

snack::debug 0
snack::sound s -debug 0

set path "nist/lib"
set fileList [glob $path/data/ex*.wav] 

foreach file $fileList { 
    puts "Playing: $file"
    s config -file $file
    s play -block 1
}
