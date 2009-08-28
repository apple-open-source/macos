if 0 {
 api_list_completion.tcl -- This automatically completes an API list from
    api_generator.tcl.

 Copyright (c) 2008 Apple Inc.
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization.
}


proc gliDispatch-name name {
    set r ""
    set ext [string match *EXT $name]

    if {$ext} {
	#Trim off the EXT suffix for appending it later.
	set name [string range $name 0 end-3]
    }

    set nv [string match *NV $name]
    
    if {$nv} {
	#Trim off the NV suffix for appending it later.
	set name [string range $name 0 end-2]
    }

    set mesa [string match *MESA $name]
   
    if {$mesa} {
	set name [string range $name 0 end-4]
    }

    set arb [string match *ARB $name]
    
    if {$arb} {
	set name [string range $name 0 end-3]
    }

    set sgix [string match *SGIX $name]
    
    if {$sgix} {
	set name [string range $name 0 end-4]
    }

    foreach c [split $name ""] {
	if {[string is upper $c]} {
	    if {"" eq $r} {
		append r [string tolower $c]
	    } else {
		append r _[string tolower $c]
	    }
	} else {
	    append r $c
	}
    }

    if {$ext} {
	append r _EXT
    }

    if {$nv} {
	append r _NV
    }

    if {$mesa} {
	append r _MESA
    }

    if {$arb} {
	append r _ARB
    }

    if {$sgix} {
	append r _SGIX
    }

    return $r
}

proc main {argv} {
    set fd [open [lindex $argv 0] r]
    set data [read $fd]
    close $fd

    set completed [list]

    foreach line [split $data \n] {
	if {[llength $line] == 5} {
	    lappend completed $line
	    continue
	}

	set type [lindex $line 0]
	set name [lindex $line 1]
	set arguments [lindex $line 2]

	set gli [gliDispatch-name $name]
	set mesa $name

	lappend completed [list $type $name $arguments $gli $mesa]
    }
    
    foreach i $completed {
	puts $i
    }
}
main $::argv
