# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/kbddata.tcl,v 1.1 1999/04/05 07:12:59 dawes Exp $
#
# Copyright 1998 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
#  Data used by the keyboard configuration routines
#

set XKBrules $Xwinhome/lib/X11/xkb/rules/xfree86

# procedure for determining available XKB settings, when the XKB extension
# is not available
#
proc Kbd_setxkbcomponents {} {
	global Xwinhome locale XKBrules XKBComponents

	set XKBComponents(models,names)		 ""
	set XKBComponents(models,descriptions)	 ""
	set XKBComponents(layouts,names)	 ""
	set XKBComponents(layouts,descriptions)	 ""
	set XKBComponents(variants,names)	 ""
	set XKBComponents(variants,descriptions) ""
	set XKBComponents(options,names)	 ""
	set XKBComponents(options,descriptions)	 ""

	if [file readable ${XKBrules}-$locale.lst] {
		set fd [open $XKBrules-$locale.lst r]
	} else {
		set fd [open $XKBrules.lst r]
	}
	set type none
	set ws  "\[ \t]"
	while { [gets $fd line] >= 0 } {
	    switch -regexp -- $line {
		"^$ws*$"	-
		"^$ws*//"	continue
		"^! [a-z]+"	{
				 set type [string range $line 2 end]
				 set type [string trim $type]
				}
		default	{
		    switch -- $type {
			model	-
			layout	-
			variant	-
			option	{
				 if ![regexp "^$ws+(\[^ \t]+)$ws+(.+)$" \
					    $line dummy name desc] \
					continue
				 if { "X$type" == "Xoption" && \
					    [string first : $name] == -1 } \
					continue
				 lappend XKBComponents(${type}s,names) \
					$name
				 lappend XKBComponents(${type}s,descriptions) \
					$desc
				}
			}
		}
	    }
	}
	if [string length $XKBComponents(models,names)] \
		return
	# Set some defaults, if we couldn't read the rules list
	set XKBComponents(models,names)		 "pc101 pc102 pc104 microsoft"
	set XKBComponents(models,descriptions)	 [list \
		"Generic 101-key PC" "Generic 102-key PC" \
		"Generic 104-key PC" "Microsoft Natural"]
	set XKBComponents(layouts,names)	 "us de es fr gb it jp"
	set XKBComponents(layouts,descriptions)	 [list \
		"U.S. English" German Spanish French \
		"U.K. English" Italian Japanese]
}


