#
#    This script activates msgcat if available, otherwise it
#    defines a minimal number of functions to make Internationalization
#    within Img possible using Tk 8.0 or higher.
#
if {[catch {package require msgcat}]} {
    if {[info exists env(LANG)]} {
	set fileName [file join [file dirname [info script]] msgs \
		[string tolower [string range $env(LANG) 0 1]].msg]
	if {[file readable $fileName]} {
	    namespace eval msgcat {
		proc mcset {args} {
		    global ::msgs
		    if {[llength $args] > 2} {
			set msgs([lindex $args 1]) [lindex $args 2]
		    }
		}
	    }
	    source $fileName
	}
    }
    proc mc {string} {
	global msgs
	if {[info exists msgs($string)]} {
	    return [set msgs($string)]
	} else {
	    return $string
	}
    }
} else {
    ::msgcat::mcload [file join [file dirname [info script]] msgs]
    catch {namespace import ::msgcat::mc}
}
