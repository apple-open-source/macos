# procepc1.tcl --
#
#  Test file for compilation.
#  Tests compiled procedure bodies when the interpreter's epoch changes.
#  Checks that changing the epoch does not trigger a recompilation.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: procepc1.tcl,v 1.2 2000/05/30 22:19:12 wart Exp $

proc a { x y } {
    set l [string tolower $x ]
    set m [string toupper $y ]

    return [list $x $l $y $m]
}

# Execute the procedure with the new epoch

set result [a 1 2]

# This statement should change the epoch in the interpreter, since
# 'break' has a compile proc.

rename break break_old

# Execute the procedure with the new epoch

lappend result [a 3 4]

# and again

rename break_old break

lappend result [a 5 6]

set result
