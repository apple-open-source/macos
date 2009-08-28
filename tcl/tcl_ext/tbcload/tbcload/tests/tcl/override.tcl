# override.tcl --
#
#  Checks that files that contain procedures with the same name as the
#  ones used by the compiler package do not cause the ones in the compiler
#  package to disappear.
#  They would if the temporary command created in CompileOneProcBody had
#  the same name as the procedure whose body we are compiling.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: override.tcl,v 1.2 2000/05/30 22:19:11 wart Exp $

# this code used to make the procomp::log proc in procomp.tcl disappear.
# The error looks like this:
#	invalid command name "log"
#	    while executing
#	"log "compiled: $path""
#	    (procedure "fileCompile" line 51)
#	    invoked from within
#	"fileCompile $file"
#	    (procedure "procomp::run" line 9)
#	    invoked from within
#	"procomp::run"
#	    (file "compiler/startup.tcl" line 43)
# If the name of the compiler package's namespce is changed, this namespace
# must be changed as well to keep the test valid.

namespace eval procomp {
    namespace export log
}

proc procomp::log { msg } {
    return "$msg : $msg"
}

procomp::log TEST
