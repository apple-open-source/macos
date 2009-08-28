# -*- tcl -*-
# ### ### ### ######### ######### #########
##

# Class for the handling of stream destinations.

# ### ### ### ######### ######### #########
## Requirements

package require snit

# ### ### ### ######### ######### #########
## Implementation

snit::type ::transfer::data::destination {

    # ### ### ### ######### ######### #########
    ## API

    #                       Destination is ...
    option -channel  {} ; # an open & writable channel.
    option -file     {} ; # a writable file.
    option -variable {} ; # the named variable.

    method put   {chunk} {}
    method done  {}      {}
    method valid {mv}    {}

    method receive {sock done} {}

    # ### ### ### ######### ######### #########
    ## Implementation

    method put {chunk} {
	if {$xtype eq "file"} {
	    set value [open $value w]
	    set xtype  channel
	    set close 1
	}

	switch -exact -- $xtype {
	    variable {
		upvar \#0 $value var
		append var $chunk
	    }
	    channel {
		puts -nonewline $value $chunk
	    }
	}
	return
    }

    method done {} {
	switch -exact -- $xtype {
	    file - variable {}
	    channel {
		if {$close} {close $value}
	    }
	}
    }

    method valid {mv} {
	upvar 1 $mv message
	switch -exact -- $xtype {
	    undefined {
		set message "Data destination is undefined"
		return 0
	    }
	    default {}
	}
	return 1
    }

    method receive {sock done} {
	set ntransfered 0
	set old [fconfigure $sock -blocking]
	fconfigure $sock -blocking 0
	fileevent $sock readable \
		[mymethod Read $sock $old $done]
	return
    }

    method Read {sock oldblock done} {
	set chunk [read $sock]
	if {[set l [string length $chunk]]} {
	    $self put $chunk
	    incr ntransfered $l
	}
	if {[eof $sock]} {
	    $self done
	    fileevent  $sock readable {}
	    fconfigure $sock -blocking $oldblock

	    lappend done $ntransfered
	    uplevel #0 $done
	}
	return
    }

    # ### ### ### ######### ######### #########
    ## Internal helper commands.

    onconfigure -variable {newvalue} {
	set etype variable
	set xtype string

	if {![uplevel \#0 {info exists $newvalue}]} {
	    return -code error "Bad variable \"$newvalue\", does not exist"
	}

	set value $newvalue
	return
    }

    onconfigure -channel {newvalue} {
	if {![llength [file channels $newvalue]]} {
	    return -code error "Bad channel handle \"$newvalue\", does not exist"
	}
	set etype channel
	set xtype channel
	set value $newvalue
	return
    }

    onconfigure -file {newvalue} {
	if {![file exists $newvalue]} {
	    set d [file dirname $newvalue]
	    if {![file writable $d]} {
		return -code error "File \"$newvalue\" not creatable"
	    }
	    if {![file isdirectory $d]} {
		return -code error "File \"$newvalue\" not creatable"
	    }
	} else {
	    if {![file writable $newvalue]} {
		return -code error "File \"$newvalue\" not writable"
	    }
	    if {![file isfile $newvalue]} {
		return -code error "File \"$newvalue\" not a file"
	    }
	}
	set etype channel
	set xtype file
	set value $newvalue
	return
    }

    # ### ### ### ######### ######### #########
    ## Data structures

    variable etype  undefined
    variable xtype  undefined
    variable value
    variable close 0

    variable ntransfered

    ##
    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide transfer::data::destination 0.1
