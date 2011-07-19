# vfsUtils.tcl --
#
# $Id: vfsUtils.tcl,v 1.28 2009/01/22 16:03:58 patthoyts Exp $

package require Tcl 8.4
package require vfs

namespace eval ::vfs {
    variable debug 0
    if {[info exists ::env(VFS_DEBUG)]} {
	set debug $::env(VFS_DEBUG)
    }
}

# This can be overridden to use a different memchan implementation
# With Tcl 8.6 will be overridden using [chan create] via vfslib.tcl
proc ::vfs::memchan {args} {
    ::package require Memchan
    uplevel 1 [list ::memchan] $args
}

# This can be overridden to use a different crc implementation
# With Tcl 8.6 will be overridden using [zlib crc32] via vfslib.tcl
proc ::vfs::crc {args} {
    ::package require crc32 ;# tcllib
    uplevel 1 [linsert [linsert $args end-1 "--"] 0 ::crc::crc32]
}

# This can be overridden to use a different zip implementation
# With Tcl 8.6 will be overridden using core zlib via vfslib.tcl
proc ::vfs::zip {args} {
    ::package require Trf
    uplevel 1 [linsert [linsert $args end-1 "--"] 0 ::zip]
}

proc ::vfs::autoMountExtension {ext cmd {pkg ""}} {
    variable extMounts
    set extMounts($ext) [list $cmd $pkg]
}

proc ::vfs::autoMountUrl {type cmd {pkg ""}} {
    variable urlMounts
    set urlMounts($type) [list $cmd $pkg]
}

proc ::vfs::log {msg {lvl 0}} {
    if {$lvl < ${::vfs::debug}} {
	#tclLog "vfs($lvl): $msg"
	puts stderr $msg
    }
}

proc ::vfs::RegisterMount {mountpoint unmountcmd} {
    variable _unmountCmd
    set _unmountCmd([file normalize $mountpoint]) $unmountcmd
}

proc ::vfs::unmount {mountpoint} {
    variable _unmountCmd
    set norm [file normalize $mountpoint]
    uplevel \#0 $_unmountCmd($norm) [list $norm]
    unset _unmountCmd($norm)
}

proc vfs::states {} {
    return [list "readwrite" "translucent" "readonly"]
}

# vfs::attributes mountpoint ?-opt val? ?...-opt val?
proc ::vfs::attributes {mountpoint args} {
    set handler [::vfs::filesystem info $mountpoint]
    
    set res {}
    
    if {[regsub -- "::handler" $handler ::attributes cmd]} {
	set attrs [eval $cmd]
    } else {
	return -code error "No known attributes"
    }

    if {![llength $args]} {
	foreach attr $attrs {
	    regsub -- "::handler" $handler ::$attr cmd
	    if {[catch $cmd val]} {
		return -code error "error reading filesystem attribute\
		  \"$attr\": $val"
	    } else {
		lappend res -$attr $val
	    }
	}
	return $res
    }
    
    while {[llength $args] > 1} {
	set attr [string range [lindex $args 0] 1 end]
	set val [lindex $args 1]
	set args [lrange $args 2 end]
	regsub -- "::handler" $handler ::$attr cmd
	if {[catch {eval $cmd [list $val]} err]} {
	    return -code error "error setting filesystem attribute\
	      \"$attr\": $err"
	} else {
	    set res $val
	}
    }
    if {[llength $args]} {
	set attr [string range [lindex $args 0] 1 end]
	regsub -- "::handler" $handler ::$attr cmd
	if {[catch $cmd val]} {
	    return -code error "error reading filesystem attribute\
	      \"$attr\": $val"
	} else {
	    set res $val
	}
    }
    return $res
}

proc vfs::attributeCantConfigure {attr val largs} {
    switch -- [llength $largs] {
	0 {
	    return $val
	}
	1 {
	    return -code error "Can't set $attr"
	}
	default {
	    return -code error "Wrong num args"
	}
    }
}

::vfs::autoMountExtension "" ::vfs::mk4::Mount vfs::mk4
::vfs::autoMountExtension .bin ::vfs::mk4::Mount vfs::mk4
::vfs::autoMountExtension .kit ::vfs::mk4::Mount vfs::mk4
::vfs::autoMountExtension .tar ::vfs::tar::Mount vfs::tar
::vfs::autoMountExtension .zip ::vfs::zip::Mount vfs::zip
::vfs::autoMountUrl ftp ::vfs::ftp::Mount vfs::ftp
::vfs::autoMountUrl file ::vfs::fileUrlMount vfs
::vfs::autoMountUrl tclns ::vfs::tclprocMount vfs::ns

proc ::vfs::haveMount {url} {
    variable mounted
    info exists mounted($url)
}

proc ::vfs::urlMount {url args} {
    ::vfs::log "$url $args"
    variable urlMounts
    if {[regexp {^([a-zA-Z]+)://(.*)} $url "" urltype rest]} {
	if {[info exists urlMounts($urltype)]} {
	    #::vfs::log "automounting $path"
	    foreach {cmd pkg} $urlMounts($urltype) {}
	    if {[string length $pkg]} {
		package require $pkg
	    }
	    eval $cmd [list $url] $args
	    variable mounted
	    set mounted($url) 1
	    return
	}
	return -code error "Unknown url type '$urltype'"
    }
    return -code error "Couldn't parse url $url"
}

proc ::vfs::fileUrlMount {url args} {
    # Strip off the leading 'file://'
    set file [string range $url 7 end]
    eval [list ::vfs::auto $file] $args
}

proc ::vfs::tclprocMount {url args} {
    # Strip off the leading 'tclns://'
    set ns [string range $url 8 end]
    eval [list ::vfs::tclproc::Mount $ns] $args
}

proc ::vfs::auto {filename args} {
    variable extMounts
    
    set np {}
    set split [::file split $filename]
    
    foreach ele $split {
	lappend np $ele
	set path [::file normalize [eval [list ::file join] $np]]
	if {[::file isdirectory $path]} {
	    # already mounted
	    continue
	} elseif {[::file isfile $path]} {
	    set ext [string tolower [::file extension $ele]]
	    if {[::info exists extMounts($ext)]} {
		#::vfs::log "automounting $path"
		foreach {cmd pkg} $extMounts($ext) {}
		if {[string length $pkg]} {
		    package require $pkg
		}
		eval $cmd [list $path $path] $args
	    } else {
		continue
	    }
	} else {
	    # It doesn't exist, so just return
	    # return -code error "$path doesn't exist"
	    return
	}
    }
}

# Helper procedure for vfs matchindirectory
# implementations.  It is very important that
# we match properly when given 'directory'
# specifications, since this is used for
# recursive globbing by Tcl.
proc vfs::matchCorrectTypes {types filelist {inDir ""}} {
    if {$types != 0} {
	# Which types to return.  We must do special
	# handling of directories and files.
	set file [matchFiles $types]
	set dir [matchDirectories $types]
	if {$file && $dir} {
	    return $filelist
	}
	if {$file == 0 && $dir == 0} {
	    return [list]
	}
	set newres [list]
	set subcmd [expr {$file ? "isfile" : "isdirectory"}]
	if {[string length $inDir]} {
	    foreach r $filelist {
		if {[::file $subcmd [file join $inDir $r]]} {
		    lappend newres $r
		}
	    }
	} else {
	    foreach r $filelist {
		if {[::file $subcmd $r]} {
		    lappend newres $r
		}
	    }
	}
	set filelist $newres
    }
    return $filelist
}

# Convert integer mode to a somewhat preferable string.
proc vfs::accessMode {mode} {
    lindex [list F X W XW R RX RW] $mode
}

proc vfs::matchDirectories {types} {
    return [expr {$types == 0 ? 1 : $types & (1<<2)}]
}

proc vfs::matchFiles {types} {
    return [expr {$types == 0 ? 1 : $types & (1<<4)}]
}

proc vfs::modeToString {mode} {
    # Turn a POSIX open 'mode' set of flags into a more readable
    # string 'r', 'w', 'w+', 'a', etc.
    set res ""
    if {$mode & 1} {
	append res "r"
    } elseif {$mode & 2} {
	if {$mode & 16} {
	    append res "w"
	} else {
	    append res "a"
	}
    }
    if {$mode & 4} {
	append res "+"
    }
    set res
}

# These lists are used to convert attribute indices into the string equivalent.
# They are copied from Tcl's C sources.  There is no need for them to be
# the same as in the native filesystem; we can use completely different
# attribute sets.  However some items, like '-longname' it is probably
# best to implement.
set vfs::attributes(windows) [list -archive -hidden -longname -readonly -shortname -system -vfs]
set vfs::attributes(macintosh) [list -creator -hidden -readonly -type -vfs]
set vfs::attributes(unix) [list -group -owner -permissions -vfs]

proc vfs::listAttributes {} {
    variable attributes
    global tcl_platform
    set attributes($tcl_platform(platform))
}

proc vfs::indexToAttribute {idx} {
    return [lindex [listAttributes] $idx]
}

proc vfs::attributesGet {root stem index} {
    # Return standard Tcl result, or error.
    set attribute [indexToAttribute $index]
    switch -- $attribute {
	"-longname" {
	    # We always use the normalized form!
	    return [file join $root $stem]
	}
	"-shortname" {
	    set rootdir [file attributes [file dirname $root] -shortname]
	    return [file join $rootdir [file tail $root] $stem]
	}
	"-archive" {
	    return 0
	}
	"-hidden" {
	    return 0
	}
	"-readonly" {
	    return 0
	}
	"-system" {
	    return 0
	}
	"-vfs" {
	    return 1
	}
	"-owner" {
	    return
	}
	"-group" {
	    return
	}
    }
}

proc vfs::attributesSet {root stem index val} {
    # Return standard Tcl result, or error.
    set attribute [indexToAttribute $index]
    #::vfs::log "$attribute"
    switch -- $attribute {
	"-owner"   -
	"-group"   -
	"-archive" -
	"-hidden"  -
	"-permissions" {
	    return
	}
	"-longname" {
	    return -code error "no such luck"
	}
	"-vfs" {
	    return -code error "read-only"
	}
    }
}

proc vfs::posixError {name} {
    variable posix
    return $posix($name)
}

set vfs::posix(EPERM)		1	;# Operation not permitted
set vfs::posix(ENOENT)		2	;# No such file or directory
set vfs::posix(ESRCH)		3	;# No such process
set vfs::posix(EINTR)		4	;# Interrupted system call
set vfs::posix(EIO)		5	;# Input/output error
set vfs::posix(ENXIO)		6	;# Device not configured
set vfs::posix(E2BIG)		7	;# Argument list too long
set vfs::posix(ENOEXEC)		8	;# Exec format error
set vfs::posix(EBADF)		9	;# Bad file descriptor
set vfs::posix(ECHILD)		10	;# No child processes
set vfs::posix(EDEADLK)		11	;# Resource deadlock avoided
					;# 11 was EAGAIN
set vfs::posix(ENOMEM)		12	;# Cannot allocate memory
set vfs::posix(EACCES)		13	;# Permission denied
set vfs::posix(EFAULT)		14	;# Bad address
set vfs::posix(ENOTBLK)		15	;# Block device required
set vfs::posix(EBUSY)		16	;# Device busy
set vfs::posix(EEXIST)		17	;# File exists
set vfs::posix(EXDEV)		18	;# Cross-device link
set vfs::posix(ENODEV)		19	;# Operation not supported by device
set vfs::posix(ENOTDIR)		20	;# Not a directory
set vfs::posix(EISDIR)		21	;# Is a directory
set vfs::posix(EINVAL)		22	;# Invalid argument
set vfs::posix(ENFILE)		23	;# Too many open files in system
set vfs::posix(EMFILE)		24	;# Too many open files
set vfs::posix(ENOTTY)		25	;# Inappropriate ioctl for device
set vfs::posix(ETXTBSY)		26	;# Text file busy
set vfs::posix(EFBIG)		27	;# File too large
set vfs::posix(ENOSPC)		28	;# No space left on device
set vfs::posix(ESPIPE)		29	;# Illegal seek
set vfs::posix(EROFS)		30	;# Read-only file system
set vfs::posix(EMLINK)		31	;# Too many links
set vfs::posix(EPIPE)		32	;# Broken pipe
set vfs::posix(EDOM)		33	;# Numerical argument out of domain
set vfs::posix(ERANGE)		34	;# Result too large
set vfs::posix(EAGAIN)		35	;# Resource temporarily unavailable
set vfs::posix(EWOULDBLOCK)	35	;# Operation would block
set vfs::posix(EINPROGRESS)	36	;# Operation now in progress
set vfs::posix(EALREADY)	37	;# Operation already in progress
set vfs::posix(ENOTSOCK)	38	;# Socket operation on non-socket
set vfs::posix(EDESTADDRREQ)	39	;# Destination address required
set vfs::posix(EMSGSIZE)	40	;# Message too long
set vfs::posix(EPROTOTYPE)	41	;# Protocol wrong type for socket
set vfs::posix(ENOPROTOOPT)	42	;# Protocol not available
set vfs::posix(EPROTONOSUPPORT)	43	;# Protocol not supported
set vfs::posix(ESOCKTNOSUPPORT)	44	;# Socket type not supported
set vfs::posix(EOPNOTSUPP)	45	;# Operation not supported on socket
set vfs::posix(EPFNOSUPPORT)	46	;# Protocol family not supported
set vfs::posix(EAFNOSUPPORT)	47	;# Address family not supported by protocol family
set vfs::posix(EADDRINUSE)	48	;# Address already in use
set vfs::posix(EADDRNOTAVAIL)	49	;# Can't assign requested address
set vfs::posix(ENETDOWN)	50	;# Network is down
set vfs::posix(ENETUNREACH)	51	;# Network is unreachable
set vfs::posix(ENETRESET)	52	;# Network dropped connection on reset
set vfs::posix(ECONNABORTED)	53	;# Software caused connection abort
set vfs::posix(ECONNRESET)	54	;# Connection reset by peer
set vfs::posix(ENOBUFS)		55	;# No buffer space available
set vfs::posix(EISCONN)		56	;# Socket is already connected
set vfs::posix(ENOTCONN)	57	;# Socket is not connected
set vfs::posix(ESHUTDOWN)	58	;# Can't send after socket shutdown
set vfs::posix(ETOOMANYREFS)	59	;# Too many references: can't splice
set vfs::posix(ETIMEDOUT)	60	;# Connection timed out
set vfs::posix(ECONNREFUSED)	61	;# Connection refused
set vfs::posix(ELOOP)		62	;# Too many levels of symbolic links
set vfs::posix(ENAMETOOLONG)	63	;# File name too long
set vfs::posix(EHOSTDOWN)	64	;# Host is down
set vfs::posix(EHOSTUNREACH)	65	;# No route to host
set vfs::posix(ENOTEMPTY)	66	;# Directory not empty
set vfs::posix(EPROCLIM)	67	;# Too many processes
set vfs::posix(EUSERS)		68	;# Too many users
set vfs::posix(EDQUOT)		69	;# Disc quota exceeded
set vfs::posix(ESTALE)		70	;# Stale NFS file handle
set vfs::posix(EREMOTE)		71	;# Too many levels of remote in path
set vfs::posix(EBADRPC)		72	;# RPC struct is bad
set vfs::posix(ERPCMISMATCH)	73	;# RPC version wrong
set vfs::posix(EPROGUNAVAIL)	74	;# RPC prog. not avail
set vfs::posix(EPROGMISMATCH)	75	;# Program version wrong
set vfs::posix(EPROCUNAVAIL)	76	;# Bad procedure for program
set vfs::posix(ENOLCK)		77	;# No locks available
set vfs::posix(ENOSYS)		78	;# Function not implemented
set vfs::posix(EFTYPE)		79	;# Inappropriate file type or format
