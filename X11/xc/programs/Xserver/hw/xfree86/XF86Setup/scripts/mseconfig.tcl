# $XConsortium: mseconfig.tcl /main/1 1996/09/21 14:14:40 kaleb $
#
#
#
#
# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/scripts/mseconfig.tcl,v 3.4 1998/03/27 23:23:07 hohndel Exp $

set clicks1 [clock clicks]

array set Pointer {
	Device		""
	Protocol	""
	BaudRate	""
	SampleRate	""
	Resolution	""
	Buttons		""
	Emulate3Buttons	""
	Emulate3Timeout	""
	ChordMiddle	""
	ClearDTR	""
	ClearRTS	""
}

set ConfigFile [xf86config_findfile]
if ![getuid] {
    if {![catch {xf86config_readfile $Xwinhome files server \
		keyboard mouse monitor device screen} tmp]} {
	if [info exists mouse] {
		set Pointer(Device) $mouse(Device)
		if [string length $mouse(Device)] {
		    if {[file exists $mouse(Device)]
			    && [file type $mouse(Device)] == "link" } {
		        set Pointer(RealDev) [readlink $mouse(Device)]
		        set Pointer(OldLink) $mouse(Device)
		    } else {
			set Pointer(RealDev) $mouse(Device)
		    }
		}
	}
	foreach var {files server keyboard mouse} {
		catch {unset $var}
	}
	foreach var [info vars monitor_*] {
		catch {unset $var}
	}
	foreach var [info vars device_*] {
		catch {unset $var}
	}
	foreach var [info vars screen_*] {
		catch {unset $var}
	}
    }
    set clicks2 [clock clicks]
}

if { [info exists env(TMPDIR)] } {
	set XF86SetupDir $env(TMPDIR)/.XF86Setup[pid]
} else {
	set XF86SetupDir /tmp/.XF86Setup[pid]
}


if ![mkdir $XF86SetupDir 0700] {
	mesg "Unable to make directory $XF86SetupDir\n\
		 for storing temporary files" okay
	exit 1
}

source $XF86Setup_library/texts/local_text.tcl
source $XF86Setup_library/setuplib.tcl
if ![getuid] {
	set rand1 [random 1073741823]
	random seed [expr $clicks2-$clicks1]
	set rand2 [random 1073741823]

	set TmpDir $XF86SetupDir/[format "%x-%x" $rand1 $rand2]
	if ![mkdir $TmpDir 0700] {
		mesg "Unable to make directory $TmpDir\n\
			 for storing temporary files" okay
		exit 1
	}
} else {
	set TmpDir $XF86SetupDir
}

check_tmpdirs

if ![getuid] {
	set Pointer(Device) $TmpDir/mouse
	if [info exists Pointer(RealDev)] {
	    link $Pointer(RealDev) $Pointer(Device)
	}
}

set_resource_defaults
source $XF86Setup_library/mseproto.tcl
source $XF86Setup_library/mouse.tcl
Mouse_create_widgets .
Mouse_activate .
button .mouse.exit -text $messages(mouse.17) \
	-command "exit 0" -underline $messages(mouse.18)
pack .mouse.exit -side bottom -expand yes -fill x
bind . <Alt-x>		"exit 0"
bind . <Control-x>	"exit 0"
