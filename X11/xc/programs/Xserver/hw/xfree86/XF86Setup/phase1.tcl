# $XFree86: xc/programs/Xserver/hw/xfree86/XF86Setup/phase1.tcl,v 3.21 1999/07/12 08:14:26 dawes Exp $
#
# Copyright 1996 by Joseph V. Moss <joe@XFree86.Org>
#
# See the file "LICENSE" for information regarding redistribution terms,
# and for a DISCLAIMER OF ALL WARRANTIES.
#
# $XConsortium: phase1.tcl /main/3 1996/10/28 05:42:26 kaleb $

#
# Phase I - Initial text mode interaction w/user and starting of SVGA server
#
set clicks1 [clock clicks]

# load the autoload stuff
source $tcl_library/init.tcl
# load language specific library
source $XF86Setup_library/texts/local_text.tcl
# load in our library
source $XF86Setup_library/setuplib.tcl
source $XF86Setup_library/filelist.tcl
source $XF86Setup_library/carddata.tcl
source $XF86Setup_library/kbddata.tcl
source $XF86Setup_library/mondata.tcl

proc check_for_files { xwinhome } {
	global FilePermsDescriptions FilePermsReadMe messages

	foreach var [array names FilePermsDescriptions] {
	    global FilePerms$var
	    foreach tmp [array names FilePerms$var] {
		set pattern [lindex $tmp 0]
		set perms   [lindex $tmp 1] ;# ignored (for now at least)
		if ![llength [glob -nocomplain -- $xwinhome/$pattern]] {
			set msg [format "$messages(phase1.1) %s %s %s %s" \
				$FilePermsDescriptions($var) \
				$messages(phase1.2) \
				$xwinhome/$pattern $messages(phase1.3)]
			mesg [parafmt 65 $msg] okay
			exit 1
		}
	    }
	}
	foreach readme [array names FilePermsReadMe] {
	    set pattern [lindex $readme 0]
	    set perms   [lindex $readme 1] ;# ignored (for now at least)
	    if ![llength [glob -nocomplain -- $xwinhome/$pattern]] {
		mesg [parafmt 65 $messages(phase1.4)] okay
		break
	    }
	}
}

proc set_xf86config_defaults {} {
    global Xwinhome ConfigFile messages
    global Files Server Keyboard Pointer MonitorIDs DeviceIDs

    if {![catch {xf86config_readfile $Xwinhome files server \
		keyboard mouse monitor device screen} tmp]} {
	array set Files		[array get files]
	array set Server	[array get Server]
	array set Keyboard	[array get keyboard]
	array set Pointer	[array get mouse]
	foreach drvr { Mono VGA2 VGA16 SVGA Accel } {
	    global Scrn_$drvr
	    if [info exists screen_$drvr] {
		array set Scrn_$drvr	[array get screen_$drvr]
	    } else {
		set Scrn_${drvr}(Driver) ""
	    }
	}
	set MonitorIDs [set DeviceIDs ""]
	set primon 0
	set priname "Primary Monitor"
	global Monitor_$priname
	foreach mon [info vars monitor_*] {
		set id [string range $mon 8 end]
		global Monitor_$id
		if { "$id" == "$priname" } {
			set primon 1
		} else {
			array set Monitor_$id [array get Monitor_$priname]
		}
		lappend MonitorIDs $id
		array set Monitor_$id	[array get monitor_$id]
	}
	if !$primon { global Monitor_$priname; unset Monitor_$priname }
	set pridev 0
	set priname "Primary Card"
	global Device_$priname
	foreach dev [info vars device_*] {
		set id [string range $dev 7 end]
		global Device_$id
		if { "$id" == "$priname" } {
			set pridev 1
		} else {
			array set Device_$id [array get Device_$priname]
		}
		lappend DeviceIDs $id
		array set Device_$id	[array get device_$id]
		set Device_${id}(Options) [set Device_${id}(Option)]
		set Device_${id}(Server) NoMatch
	}
	if !$pridev { global Device_$priname; unset Device_$priname }
	set fd [open $ConfigFile r]
	set ws      "\[ \t\]"
	set nqt     {[^"]}
	set alnum   {[A-Z0-9]}
	set idpat   "^$ws+\[Ii]\[Dd]$nqt+\"($nqt*)\""
	set servpat "\"$nqt*\"$ws+##.*SERVER:$ws*($alnum+)"
	while { [gets $fd line] >= 0 } {
	    set tmp [string toupper [zap_white $line] ]
	    if { [string compare $tmp {SECTION"DEVICE"}] == 0 } {
		while { [gets $fd nextline] >= 0 } {
		    set upper [string toupper $nextline]
		    if { [regexp $idpat $nextline dummy id] } {
			set found [regexp $servpat $upper dummy serv]
			if $found {
				if { [string match XF86_* $serv] } {
				    set serv [string range $serv 5 end]
				}
				set Device_${id}(Server) $serv
			}
			break
		    }
		    if ![string compare [string trim $upper] "ENDSECTION"] {
			break
		    }
		}
	    }
	}
	close $fd
	global ServerList
	foreach devid $DeviceIDs {
	    set varname Device_${devid}(Server)
	    if { ![info exists $varname] ||
		    [lsearch -exact $ServerList [set $varname]] < 0} {
		set filename $Xwinhome/bin/X
		for {set nlinks 0} \
			{[file exists $filename] && \
			 [file type $filename]=="link" && $nlinks<20} \
			{incr nlinks} {
		    set filename [readlink $filename]
		}
		set $varname [string range [file tail $filename] 5 end]
		if { [lsearch -exact $ServerList [set $varname]] < 0
			|| $nlinks == 20} {
		    set $varname SVGA
		}
	    }
	}
    } else {
	mesg $messages(phase1.5) okay
	puts $tmp
	exit 0
    }
}

proc parray {a {pattern *}} {
    upvar 1 $a array
    if ![array exists array] {
        error "\"$a\" isn't an array"
    }
    set maxl 0
    foreach name [lsort [array names array $pattern]] {
        if {[string length $name] > $maxl} {
            set maxl [string length $name]
        }
    }
    set maxl [expr {$maxl + [string length $a] + 2}]
    foreach name [lsort [array names array $pattern]] {
        set nameString [format %s(%s) $a $name]
        puts stdout [format "%-*s = %s" $maxl $nameString $array($name)]
    }
}

#########
puts "XF86Setup is not yet functional.  Please use xf86config for now."
exit 0

set vlist [xf86config_readfile Cfg]
foreach arr [lsort [info vars Cfg*]] {
	#parray $arr
}
xf86config_writefile test.cfg Cfg
#exit 0

if { !$NoCurses } {
	if [info exists env(TERM)] {
		if {[catch {curses init} retval]} {
			puts [format $messages(phase1.27) $env(TERM)]
			sleep 2
			set NoCurses 1
		}
	} else {
		puts $messages(phase1.28)
		sleep 2
		set NoCurses 1
	}
}

check_for_files $Xwinhome

#set ConfigFile [xf86config_findfile]
set ConfigFile XF86Config
set StartServer 1
set ReConfig 0
set UseConfigFile 0
set UseLoader 1
if !$pc98 {
    if { ![file exists $Xwinhome/bin/XF86_LOADER] } {
	set UseLoader 0
    }
} else {
    if { ![file exists $Xwinhome/bin/XF98_LOADER] } {
	set UseLoader 0
    }
}
if { [string length $ConfigFile] > 0 } {
	if [info exists env(DISPLAY)] {
		set msg [format "%s\n \n%s\n \n%s" \
			    [parafmt 65 $messages(phase1.6)] \
			    [parafmt 65 $messages(phase1.7)] \
			    $messages(phase1.8) ]
		set ReConfig [mesg $msg yesno]
	}
	if { $ReConfig } {
		set UseConfigFile 1
		set StartServer 0
		if { [getuid] != 0 } {
		    if { [mesg $messages(phase1.9) yesno] == 0 } {
		        exit 1
		    }
		}
	} else {
		if { [getuid] != 0 } {
		    mesg $messages(phase1.10) okay
		    exit 1
		}
		if !$pc98 {
		    if { !$UseLoader && ![file exists $Xwinhome/bin/XF86_SVGA] } {
			if !$NoTk {
				mesg $messages(phase1.11) okay
				set NoTk 1
			}
		    }
		} else {
		    if { !$UseLoader && ![file exists $Xwinhome/bin/XF98_EGC] \
			          && ![file exists $Xwinhome/bin/XF98_PEGC]} {
		        mesg $messages(phase1.12) okay
		        exit 1
		    }
		}
		set UseConfigFile [mesg $messages(phase1.13) yesno]
	}
	# initialize the configuration variables
	initconfig $Xwinhome

	set UseConfigFile 0
	if { $UseConfigFile } {
		set_xf86config_defaults
	}
} else {
	set ConfigFile /etc/XF86Config
	if { [getuid] != 0 } {
	    mesg $messages(phase1.14) okay
	    exit 1
	}
	if !$pc98 {
	    if { !$ReConfig && !$UseLoader
		    && ![file exists $Xwinhome/bin/XF86_SVGA] } {
		if !$NoTk {
			mesg $messages(phase1.15) okay
			set NoTk 1
		}
	    }
	} else {
	    if { !$ReConfig && !$UseLoader
		    && ![file exists $Xwinhome/bin/XF98_EGC]
		    && ![file exists $Xwinhome/bin/XF98_PEGC] } {
	        mesg $messages(phase1.16) okay
	        exit 1
	    }
	}
	# initialize the configuration variables
	initconfig $Xwinhome
}

set clicks2 [clock clicks]

if { ![getuid] } {
    if { !$UseConfigFile } {
	# Check for the SysV Xqueue mouse driver
	if { [file exists /etc/conf/pack.d/xque]
		&& [file exists /usr/lib/mousemgr] } {
	    set xque [mesg $messages(phase1.17) yesno]
	    if $xque {
		set Keyboard(Protocol)	Xqueue
		set Pointer(Protocol)	Xqueue
		set Pointer(Device)	""
	    }
	}

	# Check for the SCO OsMouse
	if { [file exists /etc/conf/pack.d/cn/class.h]
		&& [file exists /etc/conf/pack.d/ev] } {
	    set osmse [mesg $messages(phase1.18) yesno]
	    if $osmse {
		set Pointer(Protocol)	OsMouse
		set Pointer(Device)	""
	    }
	}
    }
}

set PID [pid]
if { [info exists env(TMPDIR)] } {
	set XF86SetupDir $env(TMPDIR)/.XF86Setup$PID
} else {
	set XF86SetupDir /tmp/.XF86Setup$PID
}


if ![mkdir $XF86SetupDir 0700] {
	mesg "$messages(phase1.19)$XF86SetupDir$messages(phase1.20)" okay
	exit 1
}

set rand1 [random 1073741823]
random seed [expr $clicks2-$clicks1]
set rand2 [random 1073741823]

set TmpDir $XF86SetupDir/[format "%x-%x" $rand1 $rand2]
if ![mkdir $TmpDir 0700] {
	mesg "$messages(phase1.19)$TmpDir$messages(phase1.20)" okay
	exit 1
}
check_tmpdirs

if { ![getuid] } {
	if [string length $Pointer(Device)] {
	    if {[file exists $Pointer(Device)]
		    && [file type $Pointer(Device)] == "link" } {
	        set Pointer(RealDev) [readlink $Pointer(Device)]
	        set Pointer(OldLink) $Pointer(Device)
	    	if ![string compare $Pointer(RealDev) "/dev/psaux"] {
		    set Pointer(Protocol) "PS/2"
		}
	    } else {
		set Pointer(RealDev) $Pointer(Device)
	    }
	    set Pointer(Device) $TmpDir/mouse
	}
	if [info exists Pointer(RealDev)] {
	    link $Pointer(RealDev) $Pointer(Device)
	}
}

set Confname $TmpDir/Config

if $NoTk { set StartServer 0 }

if $StartServer {
	# write out a temp XF86Config file
	if !$pc98 {
	    writeXF86Config $Confname-1 -vgamode -generic
	} else {
	    writeXF86Config $Confname-1 -vgamode
	}

	mesg $messages(phase1.23) okay

	if !$pc98 {
	    set ServerPID [start_server SVGA $Confname-1 ServerOut-1]
	} else {
	    if !$pc98_EGC {
		set ServerPID [start_server PEGC $Confname-1 ServerOut-1]
#		if {$ServerPID == 0 || $ServerPID == -1} {
#		    puts "Unable to start PEGC server!\n\
#			    try to start EGC server.\n";
#		    set pc98_EGC 1;
#		}
            } else {
		set ServerPID [start_server EGC $Confname-1 ServerOut-1]
	    }
	}

	if { $ServerPID == 0 } {
		mesg $messages(phase1.24) info
		set NoTk 1
	}
	if { $ServerPID == -1 } {
		mesg $messages(phase1.25) info
		exit 1
	}
} else {
	if !$NoTk {
		mesg $messages(phase1.26) info
	}
}

