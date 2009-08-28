# stack.tcl --
#
#	Stack implementation for Tcl.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: stack_tcl.tcl,v 1.1 2008/06/19 23:03:35 andreas_kupries Exp $

namespace eval ::struct {}

namespace eval ::struct::stack {
    # The stacks array holds all of the stacks you've made
    variable stacks
    
    # counter is used to give a unique name for unnamed stacks
    variable counter 0

    # Only export one command, the one used to instantiate a new stack
    namespace export stack_tcl
}

# ::struct::stack::stack_tcl --
#
#	Create a new stack with a given name; if no name is given, use
#	stackX, where X is a number.
#
# Arguments:
#	name	name of the stack; if null, generate one.
#
# Results:
#	name	name of the stack created

proc ::struct::stack::stack_tcl {args} {
    variable stacks
    variable counter
    
    switch -exact -- [llength [info level 0]] {
	1 {
	    # Missing name, generate one.
	    incr counter
	    set name "stack${counter}"
	}
	2 {
	    # Standard call. New empty stack.
	    set name [lindex $args 0]
	}
	default {
	    # Error.
	    return -code error \
		    "wrong # args: should be \"stack ?name?\""
	}
    }

    # FIRST, qualify the name.
    if {![string match "::*" $name]} {
        # Get caller's namespace; append :: if not global namespace.
        set ns [uplevel 1 [list namespace current]]
        if {"::" != $ns} {
            append ns "::"
        }

        set name "$ns$name"
    }
    if {[llength [info commands $name]]} {
	return -code error \
		"command \"$name\" already exists, unable to create stack"
    }

    set stacks($name) [list ]

    # Create the command to manipulate the stack
    interp alias {} $name {} ::struct::stack::StackProc $name

    return $name
}

##########################
# Private functions follow

# ::struct::stack::StackProc --
#
#	Command that processes all stack object commands.
#
# Arguments:
#	name	name of the stack object to manipulate.
#	args	command name and args for the command
#
# Results:
#	Varies based on command to perform

proc ::struct::stack::StackProc {name cmd args} {
    # Do minimal args checks here
    if { [llength [info level 0]] == 2 } {
	return -code error "wrong # args: should be \"$name option ?arg arg ...?\""
    }

    # Split the args into command and args components
    set sub _$cmd
    if { [llength [info commands ::struct::stack::$sub]] == 0 } {
	set optlist [lsort [info commands ::struct::stack::_*]]
	set xlist {}
	foreach p $optlist {
	    set p [namespace tail $p]
	    lappend xlist [string range $p 1 end]
	}
	set optlist [linsert [join $xlist ", "] "end-1" "or"]
	return -code error \
		"bad option \"$cmd\": must be $optlist"
    }

    uplevel 1 [linsert $args 0 ::struct::stack::$sub $name]
}

# ::struct::stack::_clear --
#
#	Clear a stack.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	None.

proc ::struct::stack::_clear {name} {
    set ::struct::stack::stacks($name) [list]
    return
}

# ::struct::stack::_destroy --
#
#	Destroy a stack object by removing it's storage space and 
#	eliminating it's proc.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	None.

proc ::struct::stack::_destroy {name} {
    unset ::struct::stack::stacks($name)
    interp alias {} $name {}
    return
}

# ::struct::stack::_peek --
#
#	Retrieve the value of an item on the stack without popping it.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to pop; defaults to 1
#
# Results:
#	items	top count items from the stack; if there are not enough items
#		to fulfill the request, throws an error.

proc ::struct::stack::_peek {name {count 1}} {
    variable stacks
    upvar 0  stacks($name) mystack

    if { $count < 1 } {
	return -code error "invalid item count $count"
    } elseif { $count > [llength $mystack] } {
	return -code error "insufficient items on stack to fill request"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item peeks are not
	# listified
	return [lindex $mystack end]
    }

    # Otherwise, return a list of items
    incr count -1
    return [lreverse [lrange $mystack end-$count end]]
}

# ::struct::stack::_pop --
#
#	Pop an item off a stack.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to pop; defaults to 1
#
# Results:
#	item	top count items from the stack; if the stack is empty, 
#		returns a list of count nulls.

proc ::struct::stack::_pop {name {count 1}} {
    variable stacks
    upvar 0  stacks($name) mystack

    if { $count < 1 } {
	return -code error "invalid item count $count"
    } elseif { $count > [llength $mystack] } {
	return -code error "insufficient items on stack to fill request"
    }

    if { $count == 1 } {
	# Handle this as a special case, so single item pops are not
	# listified
	set item [lindex $mystack end]
	if {$count == [llength $mystack]} {
	    set mystack [list]
	} else {
	    set mystack [lreplace [K $mystack [unset mystack]] end end]
	}
	return $item
    }

    # Otherwise, return a list of items, and remove the items from the
    # stack.
    if {$count == [llength $mystack]} {
	set result  [lreverse [K $mystack [unset mystack]]]
	set mystack [list]
    } else {
	incr count -1
	set result  [lreverse [lrange $mystack end-$count end]]
	set mystack [lreplace [K $mystack [unset mystack]] end-$count end]
    }
    return $result
}

# ::struct::stack::_push --
#
#	Push an item onto a stack.
#
# Arguments:
#	name	name of the stack object
#	args	items to push.
#
# Results:
#	None.

proc ::struct::stack::_push {name args} {
    if { [llength $args] == 0 } {
	return -code error "wrong # args: should be \"$name push item ?item ...?\""
    }
    variable stacks
    upvar 0  stacks($name) mystack
    if {[llength $args] == 1} {
        lappend mystack [lindex $args 0]
    } else {
	# 8.5: lappend mystack {*}$args
	eval [linsert $args 0 lappend mystack]
    }
    return
}

# ::struct::stack::_rotate --
#
#	Rotate the top count number of items by step number of steps.
#
# Arguments:
#	name	name of the stack object.
#	count	number of items to rotate.
#	steps	number of steps to rotate.
#
# Results:
#	None.

proc ::struct::stack::_rotate {name count steps} {
    variable stacks
    upvar 0  stacks($name) mystack
    set len [llength $mystack]
    if { $count > $len } {
	return -code error "insufficient items on stack to fill request"
    }

    # Rotation algorithm:
    # do
    #   Find the insertion point in the stack
    #   Move the end item to the insertion point
    # repeat $steps times

    set start [expr {$len - $count}]
    set steps [expr {$steps % $count}]

    if {$steps == 0} return

    for {set i 0} {$i < $steps} {incr i} {
	set item [lindex $mystack end]
	set mystack [linsert \
			 [lreplace \
			      [K $mystack [unset mystack]] \
			      end end] $start $item]
    }
    return
}

# ::struct::stack::_size --
#
#	Return the number of objects on a stack.
#
# Arguments:
#	name	name of the stack object.
#
# Results:
#	count	number of items on the stack.

proc ::struct::stack::_size {name} {
    return [llength $::struct::stack::stacks($name)]
}

# ### ### ### ######### ######### #########

proc ::struct::stack::K {x y} { set x }

if {![llength [info commands lreverse]]} {
    proc ::struct::stack::lreverse {x} {
	# assert (llength(x) > 1)
	set r [list]
	set l [llength $x]
	while {$l} { lappend r [lindex $x [incr l -1]] }
	return $r
    }
}

# ### ### ### ######### ######### #########
## Ready

namespace eval ::struct {
    # Get 'stack::stack' into the general structure namespace for
    # pickup by the main management.
    namespace import -force stack::stack_tcl
}
