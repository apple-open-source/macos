# -*- tcl -*-
# ### ### ### ######### ######### #########
##

# Class for handling of active/passive connectivity.

# ### ### ### ######### ######### #########
## Requirements

package require snit

# ### ### ### ######### ######### #########
## Implementation

snit::type ::transfer::connect {

    # ### ### ### ######### ######### #########
    ## API

    option -host localhost
    option -port 0
    option -mode active

    option -translation {}
    option -encoding    {}
    option -eofchar     {}

    method connect {command} {}

    # active:
    # - connect to host/port
    #
    # passive:
    # - listen on port for connection

    # ### ### ### ######### ######### #########
    ## Implementation

    method connect {command} {
	if {$options(-mode) eq "active"} {

	    set sock [socket $options(-host) $options(-port)]
	    $self Setup $sock

	    lappend command $self $sock
	    uplevel \#0 $command
	    return
	} else {
	    set server [socket -server [mymethod Start $command] \
			    $options(-port)]

	    return [lindex [fconfigure $server -sockname] 2]
	}
	return
    }

    method Start {command sock peerhost peerport} {
	close $server
	$self Setup $sock

	lappend command $self $sock
	uplevel \#0 $command
	return
    }

    method Setup {sock} {
	foreach o {-translation -encoding -eofchar} {
	    if {$options($o) eq ""} continue
	    fconfigure $sock $o $options($o)
	}
	return
    }

    # ### ### ### ######### ######### #########
    ## Internal helper commands.

    onconfigure -mode {newvalue} {
	upvar 0 options(-mode) value
	if {$value eq $newvalue} return
	switch -exact -- $newvalue {
	    passive - active {
		set value $newvalue
	    }
	    default {
		return -code error "Bad value \"$newvalue\", expected active, or passive"
	    }
	}
	return
    }

    # ### ### ### ######### ######### #########
    ## Data structures

    variable server {}

    ##
    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide transfer::connect 0.1
