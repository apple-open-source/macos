# tar.tcl --
#
#       Creating, extracting, and listing posix tar archives
#
# Copyright (c) 2004    Aaron Faupell <afaupell@users.sourceforge.net>
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: tar.tcl,v 1.11 2007/02/09 06:03:56 afaupell Exp $

package provide tar 0.4

namespace eval ::tar {}

proc ::tar::parseOpts {acc opts} {
    array set flags $acc
    foreach {x y} $acc {upvar $x $x}
    
    set len [llength $opts]
    set i 0
    while {$i < $len} {
        set name [string trimleft [lindex $opts $i] -]
        if {![info exists flags($name)]} {return -code error "unknown option \"$name\""}
        if {$flags($name) == 1} {
            set $name [lindex $opts [expr {$i + 1}]]
            incr i $flags($name)
        } elseif {$flags($name) > 1} {
            set $name [lrange $opts [expr {$i + 1}] [expr {$i + $flags($name)}]]
            incr i $flags($name)
        } else {
            set $name 1
        }
        incr i
    }
}

proc ::tar::pad {size} {
    set pad [expr {512 - ($size % 512)}]
    if {$pad == 512} {return 0}
    return $pad
}

proc ::tar::readHeader {data} {
    binary scan $data a100a8a8a8a12a12a8a1a100a6a2a32a32a8a8a155 \
                      name mode uid gid size mtime cksum type \
                      linkname magic version uname gname devmajor devminor prefix
                               
    foreach x {name mode type linkname magic uname gname prefix mode uid gid size mtime cksum version devmajor devminor} {
        set $x [string trim [set $x] "\x00"]
    }
    set mode [string trim $mode " \x00"]
    foreach x {uid gid size mtime cksum version devmajor devminor} {
        set $x [format %d 0[string trim [set $x] " \x00"]]
    }

    return [list name $name mode $mode uid $uid gid $gid size $size mtime $mtime \
                 cksum $cksum type $type linkname $linkname magic $magic \
                 version $version uname $uname gname $gname devmajor $devmajor \
                 devminor $devminor prefix $prefix]
}

proc ::tar::contents {file} {
    set fh [::open $file]
    while {![eof $fh]} {
        array set header [readHeader [read $fh 512]]
        if {$header(name) == ""} break
        lappend ret $header(prefix)$header(name)
        seek $fh [expr {$header(size) + [pad $header(size)]}] current
    }
    close $fh
    return $ret
}

proc ::tar::stat {tar {file {}}} {
    set fh [::open $tar]
    while {![eof $fh]} {
        array set header [readHeader [read $fh 512]]
        if {$header(name) == ""} break
        seek $fh [expr {$header(size) + [pad $header(size)]}] current
        if {$file != "" && "$header(prefix)$header(name)" != $file} {continue}
        set header(type) [string map {0 file 5 directory 3 characterSpecial 4 blockSpecial 6 fifo 2 link} $header(type)]
        set header(mode) [string range $header(mode) 2 end]
        lappend ret $header(prefix)$header(name) [list mode $header(mode) uid $header(uid) gid $header(gid) \
                    size $header(size) mtime $header(mtime) type $header(type) linkname $header(linkname) \
                    uname $header(uname) gname $header(gname) devmajor $header(devmajor) devminor $header(devminor)]
    }
    close $fh
    return $ret
}

proc ::tar::get {tar file} {
    set fh [::open $tar]
    fconfigure $fh -encoding binary -translation lf -eofchar {}
    while {![eof $fh]} {
        array set header [readHeader [read $fh 512]]
        if {$header(name) == ""} break
        set name [string trimleft $header(prefix)$header(name) /]
        if {$name == $file} {
            set file [read $fh $header(size)]
            close $fh
            return $file
        }
        seek $fh [expr {$header(size) + [pad $header(size)]}] current
    }
    close $fh
    return {}
}

proc ::tar::untar {tar args} {
    set nooverwrite 0
    set data 0
    set nomtime 0
    set noperms 0
    parseOpts {dir 1 file 1 glob 1 nooverwrite 0 nomtime 0 noperms 0} $args
    if {![info exists dir]} {set dir [pwd]}
    set pattern *
    if {[info exists file]} {
        set pattern [string map {* \\* ? \\? \\ \\\\ \[ \\\[ \] \\\]} $file]
    } elseif {[info exists glob]} {
        set pattern $glob
    }

    set ret {}
    set fh [::open $tar]
    fconfigure $fh -encoding binary -translation lf -eofchar {}
    while {![eof $fh]} {
        array set header [readHeader [read $fh 512]]
        if {$header(name) == ""} break
        set name [string trimleft $header(prefix)$header(name) /]
        if {![string match $pattern $name] || ($nooverwrite && [file exists $name])} {
            seek $fh [expr {$header(size) + [pad $header(size)]}] current
            continue
        }

        set name [file join $dir $name]
        if {![file isdirectory [file dirname $name]]} {
            file mkdir [file dirname $name]
            lappend ret [file dirname $name] {}
        }
        if {[string match {[0346]} $header(type)]} {
            set new [::open $name w+]
            fconfigure $new -encoding binary -translation lf -eofchar {}
            fcopy $fh $new -size $header(size)
            close $new
            lappend ret $name $header(size)
        } elseif {$header(type) == 5} {
            file mkdir $name
            lappend ret $name {}
        } elseif {[string match {[12]} $header(type)] && $::tcl_platform(platform) == "unix"} {
            catch {file delete $name}
            if {![catch {file link [string map {1 -hard 2 -symbolic} $header(type)] $name $header(linkname)}]} {
                lappend ret $name {}
            }
        }
        seek $fh [pad $header(size)] current
        if {![file exists $name]} continue

        if {$::tcl_platform(platform) == "unix"} {
            if {!$noperms} {
                catch {file attributes $name -permissions [string range $header(mode) 2 end]}
            }
            catch {file attributes $name -owner $header(uid) -group $header(gid)}
            catch {file attributes $name -owner $header(uname) -group $header(gname)}
        }
        if {!$nomtime} {
            file mtime $name $header(mtime)
        }
    }
    close $fh
    return $ret
}

proc ::tar::createHeader {name followlinks} {
    foreach x {linkname uname gname prefix devmajor devminor} {set $x ""}
    
    if {$followlinks} {
        file stat $name stat
    } else {
        file lstat $name stat
    }
    
    set type [string map {file 0 directory 5 characterSpecial 3 blockSpecial 4 fifo 6 link 2 socket A} $stat(type)]
    set gid [format %o $stat(gid)]
    set uid [format %o $stat(uid)]
    set mtime [format %o $stat(mtime)]
    
    if {$::tcl_platform(platform) == "unix"} {
        set uname [file attributes $name -owner]
        set gname [file attributes $name -group]
        set mode 1[file attributes $name -permissions]
        if {$stat(type) == "link"} {set linkname [file link $name]}
    } else {
        set mode 100644
        if {$stat(type) == "directory"} {set mode 100755}
    }
    
    set size 0
    if {$stat(type) == "file"} {
        set size [format %o $stat(size)]
    }
    
    set name [string trimleft $name /]
    if {[string length $name] > 255} {
        return -code error "path name over 255 chars"
    } elseif {[string length $name] > 100} {
        set prefix [string range $name 0 end-100]
        set name [string range $name end-99 end]
    }

    set header [binary format a100A8A8A8A12A12A8a1a100A6a2a32a32a8a8a155a12 \
                              $name $mode\x00 $uid\x00 $gid\x00 $size\x00 $mtime\x00 {} $type \
                              $linkname ustar\x00 00 $uname $gname $devmajor $devminor $prefix {}]

    binary scan $header c* tmp
    set cksum 0
    foreach x $tmp {incr cksum $x}

    return [string replace $header 148 155 [binary format A8 [format %o $cksum]\x00]]
}

proc ::tar::recurseDirs {files followlinks} {
    foreach x $files {
        if {[file isdirectory $x] && ([file type $x] != "link" || $followlinks)} {
            if {[set more [glob -dir $x -nocomplain *]] != ""} {
                eval lappend files [recurseDirs $more $followlinks]
            } else {
                lappend files $x
            }
        }
    }
    return $files
}

proc ::tar::writefile {in out followlinks} {
     puts -nonewline $out [createHeader $in $followlinks]
     set size 0
     if {[file type $in] == "file" || ($followlinks && [file type $in] == "link")} {
         set in [::open $in]
         fconfigure $in -encoding binary -translation lf -eofchar {}
         set size [fcopy $in $out]
         close $in
     }
     puts -nonewline $out [string repeat \x00 [pad $size]]
}

proc ::tar::create {tar files args} {
    set dereference 0
    parseOpts {dereference 0} $args
    
    set fh [::open $tar w+]
    fconfigure $fh -encoding binary -translation lf -eofchar {}
    foreach x [recurseDirs $files $dereference] {
        writefile $x $fh $dereference
    }
    puts -nonewline $fh [string repeat \x00 1024]

    close $fh
    return $tar
}

proc ::tar::add {tar files args} {
    set dereference 0
    parseOpts {dereference 0} $args
    
    set fh [::open $tar r+]
    fconfigure $fh -encoding binary -translation lf -eofchar {}
    seek $fh -1024 end

    foreach x [recurseDirs $files $dereference] {
        writefile $x $fh $dereference
    }
    puts -nonewline $fh [string repeat \x00 1024]

    close $fh
    return $tar
}

proc ::tar::remove {tar files} {
    set n 0
    while {[file exists $tar$n.tmp]} {incr n}
    set tfh [::open $tar$n.tmp w]
    set fh [::open $tar r]

    fconfigure $fh  -encoding binary -translation lf -eofchar {}
    fconfigure $tfh -encoding binary -translation lf -eofchar {}

    while {![eof $fh]} {
        array set header [readHeader [read $fh 512]]
        if {$header(name) == ""} {
            puts -nonewline $tfh [string repeat \x00 1024]
            break
        }
        set name $header(prefix)$header(name)
        set len [expr {$header(size) + [pad $header(size)]}]
        if {[lsearch $files $name] > -1} {
            seek $fh $len current
        } else {
            seek $fh -512 current
            fcopy $fh $tfh -size [expr {$len + 512}]
        }
    }

    close $fh
    close $tfh

    file rename -force $tar$n.tmp $tar
}
