# $XConsortium: misc.tcl /main/1 1996/09/21 14:15:06 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/tcllib/misc.tcl,v 3.4 1996/12/27 06:55:01 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Misc routines that could be useful outside XF86Setup
#

# remove all whitespace from the string

proc zap_white { str } {
	regsub -all "\[ \t\n\]+" $str {} str
	return $str
}


# replace all sequences of whitespace with a single space

proc squash_white { str } {
	regsub -all "\[ \t\n\]+" $str { } str
	return $str
}


# implement do { ... } while loop

proc do { commands while expression } {
	uplevel $commands
	while { [uplevel [list expr $expression]] } {
		uplevel $commands
	}
}


# break a long line into shorter lines

proc parafmt { llen string } {
	set string [string trim [squash_white $string]]
	set retval ""
	while { [string length $string] > $llen } {
		set tmp [string range $string 0 $llen]
		#puts stderr "'$string'$tmp'$retval'"
		set pos [string last " " $tmp]
		if { $pos == -1 } {
			append retval [string range $string 0 [expr $llen-1]]\n
			set string [string range $string $llen end]
			continue
		}
		if { $pos == 0 } {
			append retval [string range $string 1 [expr $llen]]\n
			set string [string range $string $llen end]
			continue
		}
		if { $pos == $llen-1 } {
			append retval [string range $string 0 [expr $llen-2]]\n
			set string [string range $string $llen end]
			continue
		}
		append retval [string range $tmp 0 [expr $pos-1]]\n
		set string [string range $string [expr $pos+1] end]
	}
	#return [string trimright $retval \n]\n$string
	return $retval$string
}


#  convert the window name to a form that can be used as a prefix to
#    to the window names of child windows
#  - basically, avoid double dot

proc winpathprefix { w } {
	if ![string compare . $w] { return "" }
	return $w
}


# return a (sorted) list with duplicate elements removed
# uses the same syntax as lsort

proc lrmdups { args } {
	set inlist [eval lsort $args]
	set retlist ""
	set lastelem "nomatch[lindex $inlist 0]"
	foreach elem $inlist {
		if [string compare $lastelem $elem] {
			lappend retlist $elem
			set lastelem $elem
		}
	}
	return $retlist
}


# return the name of the file to which the given symlink points
# if the name is a relative path, convert it to a full path
# (assumes the symlink is given as a full path)

proc readlink { linkname } {
	set fname [file readlink $linkname]
	if { ![string length $fname] 
			|| ![string compare [string index $fname 0] /] } {
		return $fname
	}
	set path [file dirname $linkname]/$fname
	regsub -all {/\./} $path / path
	return $path
}


#simple random number generator

proc random {args} {
        global RNG_seed
    
        set max 259200
        set argcnt [llength $args]
        if { $argcnt < 1 || $argcnt > 2 } {
            error "wrong # args: random limit | seed ?seedval?"
        }
        if ![string compare [lindex $args 0] seed] {
            if { $argcnt == 2 } {
                set RNG_seed [expr [lindex $args 1]%$max]
            } else {
                set RNG_seed [expr \
                    ([pid]+[clock clicks])%$max]
            }
            return
        }
        if ![info exists RNG_seed] {
            set RNG_seed [expr ([pid]+[clock clicks])%$max]
        }
        set RNG_seed [expr ($RNG_seed*7141+54773) % $max]
        return [expr int(double($RNG_seed)*[lindex $args 0]/$max)]
}

