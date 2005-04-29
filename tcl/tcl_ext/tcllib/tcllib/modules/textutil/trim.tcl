namespace eval ::textutil {
	
    namespace eval trim {
    
	variable StrU "\[ \t\]+"
	variable StrR "(${StrU})\$"
	variable StrL "^(${StrU})"

	namespace export trim trimright trimleft \
		trimPrefix trimEmpyHeading

	# This will be redefined later. We need it just to let
	# a chance for the next import subcommand to work
	#
	proc trimleft  { text { trim "[ \t]+" } } { }
	proc trimright { text { trim "[ \t]+" } } { }
	proc trim      { text { trim "[ \t]+" } } { }

	proc trimPrefix {text prefix} {}
	proc trimEmptyHeading {text} {}
    }

    namespace import -force trim::trim trim::trimleft trim::trimright trim::trimPrefix trim::trimEmpyHeading
    namespace export trim trimleft trimright trimPrefix trimEmpyHeading
}


proc ::textutil::trim::trimleft {text {trim "[ \t]+"}} {
    regsub -line -all -- [MakeStr $trim left] $text {} text
    return $text
}

proc ::textutil::trim::trimright {text {trim "[ \t]+"}} {
    regsub -line -all -- [MakeStr $trim right] $text {} text
    return $text
}

proc ::textutil::trim::trim {text {trim "[ \t]+"}} {
    regsub -line -all -- [MakeStr $trim left]  $text {} text
    regsub -line -all -- [MakeStr $trim right] $text {} text
    return $text
}

proc ::textutil::trim::MakeStr { string pos }  {
    variable StrU
    variable StrR
    variable StrL

    if { "$string" != "$StrU" } {
        set StrU $string
        set StrR "(${StrU})\$"
        set StrL "^(${StrU})"
    }
    if { "$pos" == "left" } {
        return $StrL
    }
    if { "$pos" == "right" } {
        return $StrR
    }

    return -code error "Panic, illegal position key \"$pos\""
}


# @c Strips <a prefix> from <a text>, if found at its start.
#
# @a text: The string to check for <a prefix>.
# @a prefix: The string to remove from <a text>.
#
# @r The <a text>, but without <a prefix>.
#
# @i remove, prefix

proc ::textutil::trim::trimPrefix {text prefix} {
    if {[string first $prefix $text] == 0} {
	return [string range $text [string length $prefix] end]
    } else {
	return $text
    }
}


# @c Removes the Heading Empty Lines of <a text>.
#
# @a text: The text block to manipulate.
#
# @r The <a text>, but without heading empty lines.
#
# @i remove, empty lines

proc ::textutil::trim::trimEmptyHeading {text} {
    regsub -- "^(\[ \t\]*\n)*" $text {} text
    return $text
}
