# Starkit support, see http://www.equi4.com/starkit/
# by Jean-Claude Wippler, July 2002

package provide starkit 1.3.3

package require vfs

# Starkit scripts can launched in a number of ways:
#   - wrapped or unwrapped
#   - using tclkit, or from tclsh/wish with a couple of pkgs installed
#   - with real MetaKit support, or with a read-only fake (ReadKit)
#   - as 2-file starkit deployment, or as 1-file starpack
#
# Furthermore, there are three variations:
#   current:  starkits
#   older:    VFS-based "scripted documents"
#   oldest:   pre-VFS "scripted documents"
#
# The code in here is only called directly from the current starkits.

namespace eval starkit {
    # these variables are defined after the call to starkit::startup
    # they are special in that a second call will not alter them
    # (as needed when a starkit sources others for more packages)
    variable topdir	;# root directory (while the starkit is mounted)
    variable mode 	;# startup mode (starkit, sourced, etc)

    # called from the header of a starkit
    proc header {driver args} {
	if {[catch {
	    set self [fullnormalize [info script]]

	    package require vfs::${driver}
	    eval [list ::vfs::${driver}::Mount $self $self] $args

	    uplevel [list source [file join $self main.tcl]]
	}]} {
	    panic $::errorInfo
	}
    }

    proc fullnormalize {path} {
	# SNARFED from tcllib, fileutil.
	# 8.5
	# return [file join {expand}[lrange [file split
	#    [file normalize [file join $path __dummy__]]] 0 end-1]]

	return [file dirname [file normalize [file join $path __dummy__]]]
    }

    # called from the startup script of a starkit to init topdir and auto_path
    # 2003/10/21, added in 1.3: remember startup mode in starkit::mode
    proc startup {} {
	if {![info exists starkit::mode]} { variable mode }
	set mode [_startup]
    }

    # returns how the script was launched: starkit, starpack, unwrapped, or
    # sourced (2003: also tclhttpd, plugin, or service)
    proc _startup {} {
	global argv0

	# 2003/02/11: new behavior, if starkit::topdir exists, don't disturb it
	if {![info exists starkit::topdir]} { variable topdir }

	set script [fullnormalize [info script]]
	set topdir [file dirname $script]

	if {$topdir eq [fullnormalize [info nameofexe]]} { return starpack }

	# pkgs live in the $topdir/lib/ directory
	set lib [file join $topdir lib]
	if {[file isdir $lib]} { autoextend $lib }

	set a0 [fullnormalize $argv0]
	if {$topdir eq $a0} { return starkit }
	if {$script eq $a0} { return unwrapped }

	# detect when sourced from tclhttpd
	if {[info procs ::Httpd_Server] ne ""} { return tclhttpd }

	# detect when sourced from the plugin (tentative)
	if {[info exists ::embed_args]} { return plugin }

	# detect when run as an NT service
	if {[info exists ::tcl_service]} { return service }

	return sourced
    }

    # append an entry to auto_path if it's not yet listed
    proc autoextend {dir} {
	global auto_path
	set dir [fullnormalize $dir]
	if {[lsearch $auto_path $dir] < 0} {
	    lappend auto_path $dir
	}
    }

    # remount a starkit with different options
    proc remount {args} {
	variable topdir
	foreach {drv arg} [vfs::filesystem info $topdir] { break }
	vfs::unmount $topdir

	eval [list [string map {handler Mount} $drv] $topdir $topdir] $args
    }

    # terminate with an error message, using most appropriate mechanism
    proc panic {msg} {
	if {[info commands wm] ne ""} {
	    catch { wm withdraw . }
	    tk_messageBox -icon error -message $msg -title "Fatal error"
	} elseif {[info commands ::eventlog] ne ""} {
	    eventlog error $msg
	} else {
	    puts stderr $msg
	}
	exit
    }

    # the following proc was copied from the critcl package:

    # return a platform designator, including both OS and machine
    #
    # only use first element of $tcl_platform(os) - we don't care
    # whether we are on "Windows NT" or "Windows XP" or whatever
    #
    # transforms $tcl_platform(machine) for some special cases
    #  - on SunOS, matches for sun4* are transformed to sparc
    #  - on all OS's matches for intel and i*86* are transformed to x86
    #  - on MacOS X "Power Macintosh" is transformed to ppc
    #
    proc platform {} {
        global tcl_platform
        set plat [lindex $tcl_platform(os) 0]
        set mach $tcl_platform(machine)
        switch -glob -- $mach {
            sun4* { set mach sparc }
            intel -
            i*86* { set mach x86 }
            "Power Macintosh" { set mach ppc }
        }
	switch -- $plat {
	  AIX   { set mach ppc }
	  HP-UX { set mach hppa }
	}
        return "$plat-$mach"
    }

    # load extension from a platform-specific subdirectory
    proc pload {dir name args} {
      set f [file join $dir [platform] $name[info sharedlibext]]
      uplevel 1 [linsert $args 0 load $f]
    }
}
