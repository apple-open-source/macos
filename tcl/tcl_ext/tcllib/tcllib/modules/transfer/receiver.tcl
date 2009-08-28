# -*- tcl -*-
# ### ### ### ######### ######### #########
##
# Transfer class. Reception of data.
##
# Utilizes data destination and connect components to handle the
# general/common parts.

# ### ### ### ######### ######### #########
## Requirements

package require snit
package require transfer::data::destination ; # Data destination
package require transfer::connect           ; # Connection startup

# ### ### ### ######### ######### #########
## Implementation

snit::type ::transfer::receiver {

    # ### ### ### ######### ######### #########
    ## API

    ## Data destination sub component

    delegate option -channel  to dest
    delegate option -file     to dest
    delegate option -variable to dest

    ## Connection management sub component

    delegate option -host        to conn
    delegate option -port        to conn
    delegate option -mode        to conn
    delegate option -translation to conn
    delegate option -encoding    to conn
    delegate option -eofchar     to conn

    ## Receiver configuration, and API

    option -command {}

    constructor {args} {}

    method start {} {}
    method busy  {} {}

    # ### ### ### ######### ######### #########
    ## Implementation

    constructor {args} {
	set dest [::transfer::data::destination ${selfns}::dest]
	set conn [::transfer::connect           ${selfns}::conn]
	set busy 0

	$self configurelist $args
	return
    }

    method start {} {
	if {$busy} {
	    return -code error "Object is busy"
	}

	if {![$dest valid msg]} {
	    return -code error $msg
	}

	if {$options(-command) eq ""} {
	    return -code error "Completion callback is missing"
	}

	set busy 1
	return [$conn connect [mymethod Begin]]
    }

    method busy {} {
	return $busy
    }

    # ### ### ### ######### ######### #########
    ## Internal helper commands.

    method Begin {__ sock} {
	# __ == conn
	$dest receive $sock \
		[mymethod Done $sock]
	return
    }

    method Done {sock args} {
	# args is either (n),
	#             or (n errormessage)

	set busy 0
	close $sock
	$self Complete $args
	return
    }

    method Complete {alist} {
	set     cmd $options(-command)
	lappend cmd $self
	foreach a $alist {lappend cmd $a}

	uplevel #0 $cmd
	return

	# 8.5: {*}$options(-command) {*}$alist
    }

    # ### ### ### ######### ######### #########
    ## Data structures

    variable dest
    variable conn
    variable busy

    ##
    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide transfer::receiver 0.1
