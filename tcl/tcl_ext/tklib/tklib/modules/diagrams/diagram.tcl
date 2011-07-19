## -*- tcl -*-
## (C) 2010 Andreas Kupries <andreas_kupries@users.sourceforge.net>
## BSD Licensed
# # ## ### ##### ######## ############# ######################

#
# diagram drawing package.
#

##
# # ## ### ##### ######## ############# ######################
## Requisites

package require Tcl 8.5        ; # Want the nice things it brings
				 # (dicts, {*}, etc.)
package require diagram::core  ; # Core drawing management
package require diagram::basic ; # Basic shapes.
package require snit           ; # Object framework.

# # ## ### ##### ######## ############# ######################
## Implementation

snit::type ::diagram {

    # # ## ### ##### ######## ############# ######################
    ## Public API :: Instance construction, and method routing

    constructor {canvas args} {
	install core  using diagram::core  ${selfns}::CORE  $canvas
	install basic using diagram::basic ${selfns}::BASIC $core

	set mybaseline [$core snap]

	if {![llength $args]} return
	$core draw {*}$args
	return
    }

    method reset {} {
	$core drop
	$core restore $mybaseline
	return
    }

    delegate method * to core

    # # ## ### ##### ######## ############# ######################
    ## Instance data, just two components,

    component core  ; # Fundamental drawing engine and management
    component basic ; # Fundamental shapes we can draw

    variable mybaseline

    ##
    # # ## ### ##### ######## ############# ######################
}

# # ## ### ##### ######## ############# ######################
## Ready

package provide diagram 1
