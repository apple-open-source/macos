# $XConsortium: phase3.tcl /main/1 1996/09/21 14:17:37 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/phase3.tcl,v 3.5 1998/04/05 16:15:51 robin Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#

#
# Phase III - Commands run after switching back to text mode
#     - responsible for starting second server
#

source $tcl_library/init.tcl
source $XF86Setup_library/texts/local_text.tcl
source $XF86Setup_library/setuplib.tcl
source $XF86Setup_library/carddata.tcl
source $XF86Setup_library/mondata.tcl
source $StateFileName

mesg $messages(phase3.1) info
sleep 2
writeXF86Config $Confname-2 -defaultmodes

set devid [lindex $DeviceIDs 0]
global Device_$devid
set server [set Device_${devid}(Server)]

set ServerPID [start_server $server $Confname-2 ServerOut-2 ]

if { $ServerPID == -1 } {
	set msg $messages(phase3.2)
}

if { $ServerPID == 0 } {
	set msg $messages(phase3.3)
}

if { $ServerPID < 1 } {
	mesg "$msg$messages(phase3.4)" okey
	set Phase2FallBack 1
	set ServerPID [start_server $server $Confname-1 ServerOut-1Bis]
	if { $ServerPID < 1 } {
		mesg $messages(phase3.5) info
		exit 1
	}
}

if { ![string length [set Device_${devid}(ClockChip)]] } {
	set fd [open $TmpDir/ServerOut-2 r]
	set clockrates ""
	set zerocount 0
	while {[gets $fd line] >= 0} {
		if {[regexp {\(.*: clocks: (.*)$} $line dummy clocks]} {
			set clocks [string trim [squash_white $clocks]]
			foreach clock [split $clocks] {
				lappend clockrates $clock
				if { $clock < 0.1 } {
					incr zerocount
				}
			}
		}
	}
	close $fd
	set clockcount [llength $clockrates]
	if { $clockcount != 0 && 1.0*$zerocount/$clockcount < 0.25 } {
		set Device_${devid}(Clocks) $clockrates
	}
}

