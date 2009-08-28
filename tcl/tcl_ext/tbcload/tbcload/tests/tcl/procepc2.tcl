# procepc2.tcl --
#
#  Test file for compilation.
#  Tests compiled procedure bodies when the interpreter's epoch changes.
#  Checks that renaming 'set' does not change the compiled behaviour.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procepc2.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x y } {
    set l [string tolower $x ]
    set m [string toupper $y ]

    return [list $x $l $y $m]
}

catch {
    # Execute the procedure with the new epoch

    set rv [a 1 2]

    # This statement should change the epoch in the interpreter, since
    # 'set' has a compile proc.

    rename set set_old

    proc set { args } {
	error "new set called"
    }

    # Execute the procedure with the new epoch

    set didCatch [catch {lappend rv [a 3 4]} err]

    rename set {}
    rename set_old set

    if {$didCatch} {
	error $err
    }

    set rv
} result

set result
