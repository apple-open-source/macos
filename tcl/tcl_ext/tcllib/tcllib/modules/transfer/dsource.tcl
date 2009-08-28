# -*- tcl -*-
# ### ### ### ######### ######### #########
##

# Class for the handling of stream sources.

# ### ### ### ######### ######### #########
## Requirements

package require transfer::copy ; # Data transmission core
package require snit

# ### ### ### ######### ######### #########
## Implementation

snit::type ::transfer::data::source {

    # ### ### ### ######### ######### #########
    ## API

    #                       Source is ...
    option -string   {} ; # a string.
    option -channel  {} ; # an open & readable channel.
    option -file     {} ; # a file.
    option -variable {} ; # a string held by the named variable.

    option -size -1     ; # number of characters to transfer.

    method type  {} {}
    method data  {} {}
    method size  {} {}
    method valid {mv} {}

    method transmit {sock blocksize done} {}

    # ### ### ### ######### ######### #########
    ## Implementation

    method type {} {
	return $xtype
    }

    method data {} {
	switch -exact -- $etype {
	    undefined {
		return -code error "Data source is undefined"
	    }
	    string - chan {
		return $value
	    }
	    variable {
		upvar \#0 $value thevalue
		return $thevalue
	    }
	    file {
		return [open $value r]
	    }
	}
    }

    method size {} {
	if {$options(-size) < 0} {
	    switch -exact -- $etype {
		undefined {
		    return -code error "Data source is undefined"
		}
		string {
		    return [string length $value]
		}
		variable {
		    upvar \#0 $value thevalue
		    return [string length $thevalue]
		}
		chan - file {
		    # Nothing, -1 passes through
		    # We do not use [file size] for a file, as a
		    # user-specified encoding may distort the
		    # counting.
		}
	    }
	}

	return $options(-size)
    }

    method valid {mv} {
	upvar 1 $mv message

	switch -exact -- $etype {
	    undefined {
		set message "Data source is undefined"
		return 0
	    }
	    string - variable {
		if {[$self size] > [string length [$self data]]} {
		    set message "Not enough data to transmit"
		    return 0
		}
	    }
	    chan {
		# Additional check of option ?
	    }
	    file {
		# Additional check of option ?
	    }
	}
	return 1
    }

    method transmit {sock blocksize done} {
	::transfer::copy::do \
	    [$self type] [$self data] $sock \
	    -size      [$self size] \
	    -blocksize $blocksize \
	    -command   $done
	return
    }

    # ### ### ### ######### ######### #########
    ## Internal helper commands.

    onconfigure -string {newvalue} {
	set etype string
	set xtype string
	set value $newvalue
	return
    }

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
	set etype chan
	set xtype chan
	set value $newvalue
	return
    }

    onconfigure -file {newvalue} {
	if {![file exists $newvalue]} {
	    return -code error "File \"$newvalue\" does not exist"
	}
	if {![file readable $newvalue]} {
	    return -code error "File \"$newvalue\" not readable"
	}
	if {![file isfile $newvalue]} {
	    return -code error "File \"$newvalue\" not a file"
	}
	set etype file
	set xtype chan
	set value $newvalue
	return
    }

    # ### ### ### ######### ######### #########
    ## Data structures

    variable etype undefined
    variable xtype undefined
    variable value

    ##
    # ### ### ### ######### ######### #########
}

# ### ### ### ######### ######### #########
## Ready

package provide transfer::data::source 0.1
