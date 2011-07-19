# -*- tcl -*-
# (C) 2006 Andreas Kupries <andreas_kupries@users.sourceforge.net>
##
# ###

namespace eval ::sak::animate {}

# ###

proc ::sak::animate::init {} {
    variable prefix
    variable n      0
    variable max    [llength $prefix]
    variable extend 0
}

proc ::sak::animate::next {string} {
    variable prefix
    variable n
    variable max
    Extend string

    puts -nonewline stdout \r\[[lindex $prefix $n]\]\ $string
    flush           stdout

    incr n ; if {$n >= $max} {set n 0}
    return
}

proc ::sak::animate::last {string} {
    variable clear
    Extend string

    puts  stdout \r\[$clear\]\ $string
    flush stdout
    return
}

# ###

proc ::sak::animate::Extend {sv} {
    variable extend
    upvar 1 $sv string

    set l [string length $string]
    while {[string length $string] < $extend} {append string " "}
    if {$l > $extend} {set extend $l}
    return
}

# ###

namespace eval ::sak::animate {
    namespace export init next last

    variable  prefix {
	{*   }	{*   }	{*   }	{*   }	{*   }
	{ *  }	{ *  }	{ *  }	{ *  }	{ *  }
	{  * }	{  * }	{  * }	{  * }	{  * }
	{   *}	{   *}	{   *}	{   *}	{   *}
	{  * }	{  * }	{  * }	{  * }	{  * }
	{ *  }	{ *  }	{ *  }	{ *  }	{ *  }
    }
    variable clear {    }
}

##
# ###

package provide sak::animate 1.0
