#!/bin/bash
# -*- tcl -*- \
exec tclsh8.5 "$0" ${1+"$@"}

package require Tcl 8.5

set     self      [file normalize [info script]]
set     selfdir   [file dirname $self]
set     module    [file dirname $selfdir]
lappend auto_path [file dirname $module]

package require pt::pgen
package require fileutil

set specification     [file join $module tests/data/ok/peg_peg/3_peg_itself]
set new_parser_tcl    [file join $module pt_parse_peg_tcl.tcl-NEW]
set new_parser_critcl [file join $module pt_parse_peg_c.tcl-NEW]
set me                $tcl_platform(user)
set name              PEG
set class             pt::parse::peg

# Note: Chicken'n'Egg here. The pt_pgen parser generator needs a PEG
#       parser to read the grammar definition from which to generate
#       a PEG parser.

# This problem was initially solved by using the grammar interpreter
# package pt::peg::interp together with a definition of the PEG
# grammar kept in the container package pt::peg::container::peg. That
# definition was created through manual conversion of the PE grammar.

# And we avoid getting back into the problem by writing the generated
# parser into a different file instead of overwriting the
# definition/package just used by the parser generator.

# The user has to, well, is asked to, review the results before
# replacing the working system with the newly-made code. And in case
# the user still ran into the problem, just go to the implementation
# of package pt::peg::from::peg and switch there from use of
# pt::parse::peg to the interpreter/container combination. The code
# for bootstrapping is still present, just commented out.

puts "Reading spec..."
set spec [fileutil::cat $specification]

# 1.
# Generate snit-based Tcl parser for the PEG grammar.

puts Tcl
fileutil::writeFile $new_parser_tcl \
    [pt::pgen \
	 peg  $spec \
	 snit \
	 -name  $name \
	 -user  $me \
	 -file  [file tail $specification] \
	 -class ${class}_tcl \
	 -package ${class}_tcl \
	]

# 2.
# Generate critcl-based C parser for the PEG grammar.

puts CriTcl
fileutil::writeFile $new_parser_critcl \
    [pt::pgen \
	 peg  $spec \
	 critcl \
	 -name    $name \
	 -user    $me \
	 -file    [file tail $specification] \
	 -class   $class \
	 -package [string map {:: _} $class]_c \
	]

exit
