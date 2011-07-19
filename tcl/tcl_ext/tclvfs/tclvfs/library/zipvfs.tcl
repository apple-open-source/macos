# Removed provision of the backward compatible name. Moved to separate
# file/package.
package provide vfs::zip 1.0.3

package require vfs

# Using the vfs, memchan and Trf extensions, we ought to be able
# to write a Tcl-only zip virtual filesystem.  What we have below
# is basically that.

namespace eval vfs::zip {}

# Used to execute a zip archive.  This is rather like a jar file
# but simpler.  We simply mount it and then source a toplevel
# file called 'main.tcl'.
proc vfs::zip::Execute {zipfile} {
    Mount $zipfile $zipfile
    source [file join $zipfile main.tcl]
}

proc vfs::zip::Mount {zipfile local} {
    set fd [::zip::open [::file normalize $zipfile]]
    vfs::filesystem mount $local [list ::vfs::zip::handler $fd]
    # Register command to unmount
    vfs::RegisterMount $local [list ::vfs::zip::Unmount $fd]
    return $fd
}

proc vfs::zip::Unmount {fd local} {
    vfs::filesystem unmount $local
    ::zip::_close $fd
}

proc vfs::zip::handler {zipfd cmd root relative actualpath args} {
    #::vfs::log [list $zipfd $cmd $root $relative $actualpath $args]
    if {$cmd == "matchindirectory"} {
	eval [list $cmd $zipfd $relative $actualpath] $args
    } else {
	eval [list $cmd $zipfd $relative] $args
    }
}

proc vfs::zip::attributes {zipfd} { return [list "state"] }
proc vfs::zip::state {zipfd args} {
    vfs::attributeCantConfigure "state" "readonly" $args
}

# If we implement the commands below, we will have a perfect
# virtual file system for zip files.

proc vfs::zip::matchindirectory {zipfd path actualpath pattern type} {
    #::vfs::log [list matchindirectory $path $actualpath $pattern $type]

    # This call to zip::getdir handles empty patterns properly as asking
    # for the existence of a single file $path only
    set res [::zip::getdir $zipfd $path $pattern]
    #::vfs::log "got $res"
    if {![string length $pattern]} {
	if {![::zip::exists $zipfd $path]} { return {} }
	set res [list $actualpath]
	set actualpath ""
    }

    set newres [list]
    foreach p [::vfs::matchCorrectTypes $type $res $actualpath] {
	lappend newres [file join $actualpath $p]
    }
    #::vfs::log "got $newres"
    return $newres
}

proc vfs::zip::stat {zipfd name} {
    #::vfs::log "stat $name"
    ::zip::stat $zipfd $name sb
    #::vfs::log [array get sb]
    array get sb
}

proc vfs::zip::access {zipfd name mode} {
    #::vfs::log "zip-access $name $mode"
    if {$mode & 2} {
	vfs::filesystem posixerror $::vfs::posix(EROFS)
    }
    # Readable, Exists and Executable are treated as 'exists'
    # Could we get more information from the archive?
    if {[::zip::exists $zipfd $name]} {
	return 1
    } else {
	error "No such file"
    }
    
}

proc vfs::zip::open {zipfd name mode permissions} {
    #::vfs::log "open $name $mode $permissions"
    # return a list of two elements:
    # 1. first element is the Tcl channel name which has been opened
    # 2. second element (optional) is a command to evaluate when
    #    the channel is closed.

    switch -- $mode {
	"" -
	"r" {
	    if {![::zip::exists $zipfd $name]} {
		vfs::filesystem posixerror $::vfs::posix(ENOENT)
	    }
	    
	    ::zip::stat $zipfd $name sb

	    set nfd [vfs::memchan]
	    fconfigure $nfd -translation binary

	    seek $zipfd $sb(ino) start
	    set data [zip::Data $zipfd sb 0]

	    puts -nonewline $nfd $data

	    fconfigure $nfd -translation auto
	    seek $nfd 0
	    return [list $nfd]
	}
	default {
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
    }
}

proc vfs::zip::createdirectory {zipfd name} {
    #::vfs::log "createdirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::zip::removedirectory {zipfd name recursive} {
    #::vfs::log "removedirectory $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::zip::deletefile {zipfd name} {
    #::vfs::log "deletefile $name"
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

proc vfs::zip::fileattributes {zipfd name args} {
    #::vfs::log "fileattributes $args"
    switch -- [llength $args] {
	0 {
	    # list strings
	    return [list]
	}
	1 {
	    # get value
	    set index [lindex $args 0]
	    return ""
	}
	2 {
	    # set value
	    set index [lindex $args 0]
	    set val [lindex $args 1]
	    vfs::filesystem posixerror $::vfs::posix(EROFS)
	}
    }
}

proc vfs::zip::utime {fd path actime mtime} {
    vfs::filesystem posixerror $::vfs::posix(EROFS)
}

# Below copied from TclKit distribution

#
# ZIP decoder:
#
# See the ZIP file format specification:
#   http://www.pkware.com/documents/casestudies/APPNOTE.TXT
#
# Format of zip file:
# [ Data ]* [ TOC ]* EndOfArchive
#
# Note: TOC is refered to in ZIP doc as "Central Archive"
#
# This means there are two ways of accessing:
#
# 1) from the begining as a stream - until the header
#	is not "PK\03\04" - ideal for unzipping.
#
# 2) for table of contents without reading entire
#	archive by first fetching EndOfArchive, then
#	just loading the TOC
#

namespace eval zip {
    array set methods {
	0	{stored - The file is stored (no compression)}
	1	{shrunk - The file is Shrunk}
	2	{reduce1 - The file is Reduced with compression factor 1}
	3	{reduce2 - The file is Reduced with compression factor 2}
	4	{reduce3 - The file is Reduced with compression factor 3}
	5	{reduce4 - The file is Reduced with compression factor 4}
	6	{implode - The file is Imploded}
	7	{reserved - Reserved for Tokenizing compression algorithm}
	8	{deflate - The file is Deflated}
	9	{reserved - Reserved for enhanced Deflating}
	10	{pkimplode - PKWARE Date Compression Library Imploding}
        11	{reserved - Reserved by PKWARE}
        12	{bzip2 - The file is compressed using BZIP2 algorithm}
        13	{reserved - Reserved by PKWARE}
        14	{lzma - LZMA (EFS)}
        15	{reserved - Reserved by PKWARE}
    }
    # Version types (high-order byte)
    array set systems {
	0	{dos}
	1	{amiga}
	2	{vms}
	3	{unix}
	4	{vm cms}
	5	{atari}
	6	{os/2}
	7	{macos}
	8	{z system 8}
	9	{cp/m}
	10	{tops20}
	11	{windows}
	12	{qdos}
	13	{riscos}
	14	{vfat}
	15	{mvs}
	16	{beos}
	17	{tandem}
	18	{theos}
    }
    # DOS File Attrs
    array set dosattrs {
	1	{readonly}
	2	{hidden}
	4	{system}
	8	{unknown8}
	16	{directory}
	32	{archive}
	64	{unknown64}
	128	{normal}
    }

    proc u_short {n}  { return [expr { ($n+0x10000)%0x10000 }] }
}

proc zip::DosTime {date time} {
    set time [u_short $time]
    set date [u_short $date]

    # time = fedcba9876543210
    #        HHHHHmmmmmmSSSSS (sec/2 actually)

    # data = fedcba9876543210
    #        yyyyyyyMMMMddddd

    set sec  [expr { ($time & 0x1F) * 2 }]
    set min  [expr { ($time >> 5) & 0x3F }]
    set hour [expr { ($time >> 11) & 0x1F }]

    set mday [expr { $date & 0x1F }]
    set mon  [expr { (($date >> 5) & 0xF) }]
    set year [expr { (($date >> 9) & 0xFF) + 1980 }]

    # Fix up bad date/time data, no need to fail
    while {$sec  > 59} {incr sec  -60}
    while {$min  > 59} {incr sec  -60}
    while {$hour > 23} {incr hour -24}
    if {$mday < 1}  {incr mday}
    if {$mon  < 1}  {incr mon}
    while {$mon > 12} {incr hour -12}

    while {[catch {
	set dt [format {%4.4d-%2.2d-%2.2d %2.2d:%2.2d:%2.2d} \
		    $year $mon $mday $hour $min $sec]
	set res [clock scan $dt -gmt 1]
    }]} {
	# Only mday can be wrong, at end of month
	incr mday -1
    }
    return $res
}


proc zip::Data {fd arr verify} {
    upvar 1 $arr sb

    # APPNOTE A: Local file header
    set buf [read $fd 30]
    set n [binary scan $buf A4sssssiiiss \
               hdr sb(ver) sb(flags) sb(method) time date \
               crc csize size namelen xtralen]

    if { ![string equal "PK\03\04" $hdr] } {
	binary scan $hdr H* x
	return -code error "bad header: $x"
    }
    set sb(ver)	   [expr {$sb(ver) & 0xffff}]
    set sb(flags)  [expr {$sb(flags) & 0xffff}]
    set sb(method) [expr {$sb(method) & 0xffff}]
    set sb(mtime)  [DosTime $date $time]
    if {!($sb(flags) & (1<<3))} {
        set sb(crc)    [expr {$crc & 0xffffffff}]
        set sb(csize)  [expr {$csize & 0xffffffff}]
        set sb(size)   [expr {$size & 0xffffffff}]
    }

    set sb(name)   [read $fd [expr {$namelen & 0xffff}]]
    set sb(extra)  [read $fd [expr {$xtralen & 0xffff}]]
    if {$sb(flags) & (1 << 10)} {
        set sb(name) [encoding convertfrom utf-8 $sb(name)]
    }
    set sb(name) [string trimleft $sb(name) "./"]

    # APPNOTE B: File data
    #   if bit 3 of flags is set the csize comes from the central directory
    set data [read $fd $sb(csize)]

    # APPNOTE C: Data descriptor
    if { $sb(flags) & (1<<3) } {
        binary scan [read $fd 4] i ddhdr
        if {($ddhdr & 0xffffffff) == 0x08074b50} {
            binary scan [read $fd 12] iii sb(crc) sb(csize) sb(size)
        } else {
            set sb(crc) $ddhdr
            binary scan [read $fd 8] ii sb(csize) sb(size)
        }
        set sb(crc) [expr {$sb(crc) & 0xffffffff}]
        set sb(csize) [expr {$sb(csize) & 0xffffffff}]
        set sb(size) [expr {$sb(size) & 0xffffffff}]
    }
    
    switch -exact -- $sb(method) {
        0 {
            # stored; no compression
        }
        8 {
            # deflated
            if {[catch {
                set data [vfs::zip -mode decompress -nowrap 1 $data]
            } err]} then {
                return -code error "error inflating \"$sb(name)\": $err"
            }
        }
        default {
            set method $sb(method)
            if {[info exists methods($method)]} {
                set method $methods($method)
            }
            return -code error "unsupported compression method
                \"$method\" used for \"$sb(name)\""
        }
    }

    if { $verify && $sb(method) != 0} {
	set ncrc [vfs::crc $data]
	if { ($ncrc & 0xffffffff) != $sb(crc) } {
	    vfs::log [format {%s: crc mismatch: expected 0x%x, got 0x%x} \
                          $sb(name) $sb(crc) $ncrc]
	}
    }
    return $data
}

proc zip::EndOfArchive {fd arr} {
    upvar 1 $arr cb

    # [SF Tclvfs Bug 1003574]. Do not seek over beginning of file.
    seek $fd 0 end

    # Just looking in the last 512 bytes may be enough to handle zip
    # archives without comments, however for archives which have
    # comments the chunk may start at an arbitrary distance from the
    # end of the file. So if we do not find the header immediately
    # we have to extend the range of our search, possibly until we
    # have a large part of the archive in memory. We can fail only
    # after the whole file has been searched.

    set sz  [tell $fd]
    set len 512
    set at  512
    while {1} {
	if {$sz < $at} {set n -$sz} else {set n -$at}

	seek $fd $n end
	set hdr [read $fd $len]

	# We are using 'string last' as we are searching the first
	# from the end, which is the last from the beginning. See [SF
	# Bug 2256740]. A zip archive stored in a zip archive can
	# confuse the unmodified code, triggering on the magic
	# sequence for the inner, uncompressed archive.
	set pos [string last "PK\05\06" $hdr]
	if {$pos == -1} {
	    if {$at >= $sz} {
		return -code error "no header found"
	    }
	    set len 540 ; # after 1st iteration we force overlap with last buffer
	    incr at 512 ; # to ensure that the pattern we look for is not split at
	    #           ; # a buffer boundary, nor the header itself
	} else {
	    break
	}
    }

    set hdr [string range $hdr [expr {$pos + 4}] [expr {$pos + 21}]]
    set pos [expr {[tell $fd] + $pos - 512}]

    binary scan $hdr ssssiis \
	cb(ndisk) cb(cdisk) \
	cb(nitems) cb(ntotal) \
	cb(csize) cb(coff) \
	cb(comment)

    set cb(ndisk)	[u_short $cb(ndisk)]
    set cb(nitems)	[u_short $cb(nitems)]
    set cb(ntotal)	[u_short $cb(ntotal)]
    set cb(comment)	[u_short $cb(comment)]

    # Compute base for situations where ZIP file
    # has been appended to another media (e.g. EXE)
    set cb(base)	[expr { $pos - $cb(csize) - $cb(coff) }]
}

proc zip::TOC {fd arr} {
    upvar 1 $arr sb

    set buf [read $fd 46]

    binary scan $buf A4ssssssiiisssssii hdr \
      sb(vem) sb(ver) sb(flags) sb(method) time date \
      sb(crc) sb(csize) sb(size) \
      flen elen clen sb(disk) sb(attr) \
      sb(atx) sb(ino)

    if { ![string equal "PK\01\02" $hdr] } {
	binary scan $hdr H* x
	return -code error "bad central header: $x"
    }

    foreach v {vem ver flags method disk attr} {
	set sb($v) [expr {$sb($v) & 0xffff}]
    }
    set sb(crc) [expr {$sb(crc) & 0xffffffff}]
    set sb(csize) [expr {$sb(csize) & 0xffffffff}]
    set sb(size) [expr {$sb(size) & 0xffffffff}]
    set sb(mtime) [DosTime $date $time]
    set sb(mode) [expr { ($sb(atx) >> 16) & 0xffff }]
    if { ( $sb(atx) & 0xff ) & 16 } {
	set sb(type) directory
    } else {
	set sb(type) file
    }
    set sb(name) [read $fd [u_short $flen]]
    set sb(extra) [read $fd [u_short $elen]]
    set sb(comment) [read $fd [u_short $clen]]
    if {$sb(flags) & (1 << 10)} {
        set sb(name) [encoding convertfrom utf-8 $sb(name)]
        set sb(comment) [encoding convertfrom utf-8 $sb(comment)]
    }
    set sb(name) [string trimleft $sb(name) "./"]
}

proc zip::open {path} {
    #vfs::log [list open $path]
    set fd [::open $path]
    
    if {[catch {
	upvar #0 zip::$fd cb
	upvar #0 zip::$fd.toc toc

	fconfigure $fd -translation binary ;#-buffering none
	
	zip::EndOfArchive $fd cb

	seek $fd $cb(coff) start

	set toc(_) 0; unset toc(_); #MakeArray
	
	for {set i 0} {$i < $cb(nitems)} {incr i} {
	    zip::TOC $fd sb
	    
	    set sb(depth) [llength [file split $sb(name)]]
	    
	    set name [string tolower $sb(name)]
	    set toc($name) [array get sb]
	    FAKEDIR toc [file dirname $name]
	}
    } err]} {
	close $fd
	return -code error $err
    }

    return $fd
}

proc zip::FAKEDIR {arr path} {
    upvar 1 $arr toc

    if { $path == "."} { return }


    if { ![info exists toc($path)] } {
	# Implicit directory
	lappend toc($path) \
		name $path \
		type directory mtime 0 size 0 mode 0777 \
		ino -1 depth [llength [file split $path]]
    }
    FAKEDIR toc [file dirname $path]
}

proc zip::exists {fd path} {
    #::vfs::log "$fd $path"
    if {$path == ""} {
	return 1
    } else {
	upvar #0 zip::$fd.toc toc
	info exists toc([string tolower $path])
    }
}

proc zip::stat {fd path arr} {
    upvar #0 zip::$fd.toc toc
    upvar 1 $arr sb
    #vfs::log [list stat $fd $path $arr [info level -1]]

    set name [string tolower $path]
    if { $name == "" || $name == "." } {
	array set sb {
	    type directory mtime 0 size 0 mode 0777 
	    ino -1 depth 0 name ""
	}
    } elseif {![info exists toc($name)] } {
	return -code error "could not read \"$path\": no such file or directory"
    } else {
	array set sb $toc($name)
    }
    set sb(dev) -1
    set sb(uid)	-1
    set sb(gid)	-1
    set sb(nlink) 1
    set sb(atime) $sb(mtime)
    set sb(ctime) $sb(mtime)
    return ""
}

# Treats empty pattern as asking for a particular file only
proc zip::getdir {fd path {pat *}} {
    #::vfs::log [list getdir $fd $path $pat]
    upvar #0 zip::$fd.toc toc

    if { $path == "." || $path == "" } {
	set path [set tmp [string tolower $pat]]
    } else {
        set globmap [list "\[" "\\\[" "*" "\\*" "?" "\\?"]
	set tmp [string tolower $path]
        set path [string map $globmap $tmp]
	if {$pat != ""} {
	    append tmp /[string tolower $pat]
	    append path /[string tolower $pat]
	}
    }
    # file split can be confused by the glob quoting so split tmp string
    set depth [llength [file split $tmp]]

    #vfs::log "getdir $fd $path $depth $pat [array names toc $path]"
    if {$depth} {
	set ret {}
	foreach key [array names toc $path] {
	    if {[string index $key end] == "/"} {
		# Directories are listed twice: both with and without
		# the trailing '/', so we ignore the one with
		continue
	    }
	    array set sb $toc($key)

	    if { $sb(depth) == $depth } {
		if {[info exists toc(${key}/)]} {
		    array set sb $toc(${key}/)
		}
		lappend ret [file tail $sb(name)]
	    } else {
		#::vfs::log "$sb(depth) vs $depth for $sb(name)"
	    }
	    unset sb
	}
	return $ret
    } else {
	# just the 'root' of the zip archive.  This obviously exists and
	# is a directory.
	return [list {}]
    }
}

proc zip::_close {fd} {
    variable $fd
    variable $fd.toc
    unset $fd
    unset $fd.toc
    ::close $fd
}
