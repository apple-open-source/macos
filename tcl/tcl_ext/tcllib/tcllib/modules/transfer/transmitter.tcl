# -*- tcl -*-
# ### ### ### ######### ######### #########
##
# Transfer class. Sending of data.
##
# Utilizes data source and connect components to handle the
# general/common parts.

# ### ### ### ######### ######### #########
## Requirements

package require snit
package require transfer::data::source ; # Data source
package require transfer::connect      ; # Connection startup

# ### ### ### ######### ######### #########
## Implementation

snit::type ::transfer::transmitter {

    # ### ### ### ######### ######### #########
    ## API

    ## Data source sub component

    delegate option -string   to source
    delegate option -channel  to source
    delegate option -file     to source
    delegate option -variable to source
    delegate option -size     to source

    ## Connection management sub component

    delegate option -host        to conn
    delegate option -port        to conn
    delegate option -mode        to conn
    delegate option -translation to conn
    delegate option -encoding    to conn
    delegate option -eofchar     to conn

    ## Transmitter configuration, and API

    option -command   {}
    option -blocksize 1024

    constructor {args} {}

    method start {} {}
    method busy  {} {}

    # ### ### ### ######### ######### #########
    ## Implementation

    constructor {args} {
	set source [::transfer::data::source ${selfns}::source]
	set conn   [::transfer::connect      ${selfns}::conn]
	set busy   0

	$self configurelist $args
	return
    }

    method start {} {
	if {$busy} {
	    return -code error "Object is busy"
	}

	if {![$source valid msg]} {
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
	# __ <=> conn
	$source transmit $sock \
		$options(-blocksize) \
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

    variable source
    variable conn
    variable busy

    ##
    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide transfer::transmitter 0.1
