# Remnants of what used to be VFS init. This uses either the 8.6 core zlib
# command or the tclkit zlib package with rechan to provide a memory channel
# and a streaming decompression channel transform.

package require Tcl 8.4; # vfs is all new for 8.4
package provide vfslib 1.4

# use zlib to define zip and crc if available
if {[llength [info command zlib]] || ![catch {load "" zlib}]} {
    proc vfs::zip {flag value args} {
	switch -glob -- "$flag $value" {
	    {-mode d*} { set mode decompress }
	    {-mode c*} { set mode compress }
	    default { error "usage: zip -mode {compress|decompress} data" }
	}
	# kludge to allow "-nowrap 1" as second option, 5-9-2002
	if {[llength $args] > 2 && [lrange $args 0 1] eq "-nowrap 1"} {
	    if {$mode eq "compress"} {
		set mode deflate
	    } else {
		set mode inflate
	    }
	}
	return [zlib $mode [lindex $args end]]
    }

    proc vfs::crc {data} {
	return [zlib crc32 $data]
    }
}

# Use 8.6 reflected channels or the rechan package in earlier versions to
# provide a memory channel implementation.
#
if {[info command ::chan] ne {}} {

    # As the core zlib channel stacking make non-seekable channels we cannot
    # implement vfs::zstream and this feature is disabled in tclkit boot.tcl
    # when the command is not present (it is only used by mk4vfs)
    #
    #proc vfs::zstream {mode ifd clen ilen} {
    #    return -code error "vfs::zstream is unsupported with core zlib"
    #}
    proc vfs::memchan {{filename {}}} {
        return [chan create {read write} \
                    [list [namespace origin _memchan_handler] $filename]]
    }
    proc vfs::_memchan_handler {filename cmd chan args} {
        upvar #0 ::vfs::_memchan(buf,$chan) buf
        upvar #0 ::vfs::_memchan(pos,$chan) pos
        upvar #0 ::vfs::_memchan(name,$chan) name
        upvar #0 ::vfs::_memchan(timer) timer
        switch -exact -- $cmd {
            initialize {
                foreach {mode} $args break
                set buf ""
                set pos 0
                set watch {}
                set name $filename
                if {![info exists timer]} { set timer "" }
                return {initialize finalize watch read write seek cget cgetall}
            }
            finalize {
                unset buf pos name
            }
            seek {
                foreach {offset base} $args break
                switch -exact -- $base {
                    current { incr offset $pos }
                    end     { incr offset [string length $buf] }
                }
                if {$offset < 0} {
                    return -code error "error during seek on \"$chan\":\
                        invalid argument"
                } elseif {$offset > [string length $buf]} {
                    set extend [expr {$offset - [string length $buf]}]
                    append buf [binary format @$extend]
                }
                return [set pos $offset]
            }
            read {
                foreach {count} $args break
                set r [string range $buf $pos [expr {$pos + $count - 1}]]
                incr pos [string length $r]
                return $r
            }
            write {
                foreach {data} $args break
		set count [string length $data]
		if { $pos >= [string length $buf] } {
		    append buf $data
		} else {
		    set last [expr { $pos + $count - 1 }]
		    set buf [string replace $buf $pos $last $data]
		}
		incr pos $count
		return $count
            }
            cget {
                foreach {option} $args break
                switch -exact -- $option {
                    -length { return [string length $buf] }
                    -allocated { return [string length $buf] }
                    default {
                        return -code error "bad option \"$option\":\
                            should be one of -blocking, -buffering,\
                            -buffersize, -encoding, -eofchar, -translation,\
                            -length or -allocated" 
                    }
                }
            }
            cgetall {
                return [list -length [string length $buf] \
                            -allocated [string length $buf]]
            }
            watch {
                foreach {eventspec} $args break
                after cancel $timer
                foreach event {read write} {
                    upvar #0 ::vfs::_memchan(watch,$event) watch
                    if {![info exists watch]} { set watch {} }
                    set ndx [lsearch -exact $watch $chan]
                    if {$event in $eventspec} {
                        if {$ndx == -1} { lappend watch $chan }
                    } else {
                        if {$ndx != -1} {
                            set watch [lreplace $watch $ndx $ndx]
                        }
                    }
                }
                set timer [after 10 [list ::vfs::_memchan_timer]]
            }
        }
    }
    # memchan channels are always writable and always readable
    proc ::vfs::_memchan_timer {} {
        set continue 0
        foreach event {read write} {
            upvar #0 ::vfs::_memchan(watch,$event) watch
            incr continue [llength $watch]
            foreach chan $watch { chan postevent $chan $event }
        }
        if {$continue > 0} {
            set ::vfs::_memchan(timer) [after 10 [info level 0]]
        }
    }

} elseif {[info command rechan] ne "" || ![catch {load "" rechan}]} {

    proc vfs::memchan_handler {cmd fd args} {
	upvar 1 ::vfs::_memchan_buf($fd) buf
	upvar 1 ::vfs::_memchan_pos($fd) pos
        upvar 1 ::vfs::_memchan_nam($fd) nam
	set arg1 [lindex $args 0]

	switch -- $cmd {
	    seek {
		switch [lindex $args 1] {
		    1 - current { incr arg1 $pos }
		    2 - end { incr arg1 [string length $buf]}
		}
		return [set pos $arg1]
	    }
	    read {
		set r [string range $buf $pos [expr { $pos + $arg1 - 1 }]]
		incr pos [string length $r]
		return $r
	    }
	    write {
		set n [string length $arg1]
		if { $pos >= [string length $buf] } {
		    append buf $arg1
		} else { # the following doesn't work yet :(
		    set last [expr { $pos + $n - 1 }]
		    set buf [string replace $buf $pos $last $arg1]
		    error "vfs memchan: sorry no inline write yet"
		}
		incr pos $n
		return $n
	    }
	    close {
		unset buf pos nam
	    }
	    default { error "bad cmd in memchan_handler: $cmd" }
	}
    }

    proc vfs::memchan {{filename {}}} {
	set fd [rechan ::vfs::memchan_handler 6]
	set ::vfs::_memchan_buf($fd) ""
	set ::vfs::_memchan_pos($fd) 0
        set ::vfs::_memchan_nam($fd) $filename
	return $fd
    }

    proc vfs::zstream_handler {zcmd ifd clen ilen imode cmd fd {a1 ""} {a2 ""}} {
	#puts stderr "z $zcmd $ifd $ilen $cmd $fd $a1 $a2"
	upvar ::vfs::_zstream_pos($fd) pos

	switch -- $cmd {
	    seek {
		switch $a2 {
		    1 - current { incr a1 $pos }
		    2 - end { incr a1 $ilen }
		}
		# to seek back, rewind, i.e. start from scratch
		if {$a1 < $pos} {
		    rename $zcmd ""
		    zlib $imode $zcmd
		    seek $ifd 0
		    set pos 0
		}
		# consume data while not yet at seek position
		while {$pos < $a1} {
		    set n [expr {$a1 - $pos}]
		    if {$n > 4096} { set n 4096 }
		    # 2003-02-09: read did not work (?), spell it out instead
		    #read $fd $n
		    zstream_handler $zcmd $ifd $clen $ilen $imode read $fd $n
		}
		return $pos
	    }
	    read {
		set r ""
		set n $a1
		#puts stderr " want $n z $zcmd pos $pos ilen $ilen"
		if {$n + $pos > $ilen} { set n [expr {$ilen - $pos}] }
		while {$n > 0} {
		    if {[$zcmd fill] == 0} {
		        set c [expr {$clen - [tell $ifd]}]
			if {$c > 4096} { set c 4096 }
			set data [read $ifd $c]
			#puts "filled $c [string length $data]"
			$zcmd fill $data
		    }
		    set data [$zcmd drain $n]
		    #puts stderr " read [string length $data]"
		    if {$data eq ""} break
		    append r $data
		    incr pos [string length $data]
		    incr n -[string length $data]
		}
		return $r
	    }
	    close {
		rename $zcmd ""
		close $ifd
		unset pos
	    }
	    default { error "bad cmd in zstream_handler: $cmd" }
	}
    }

    variable ::vfs::zseq 0	;# used to generate temp zstream cmd names

    # vfs::zstream -- 
    #
    #  Create a read-only seekable compressed channel using rechan and
    #  the streaming mode of tclkit's zlib extension.
    #
    #	  mode - compress or decompress
    #	  ifd  - input channel (should be binary)
    #	  clen - size of compressed data in bytes
    #	  ilen - size of decompressed data in bytes
    #
    proc vfs::zstream {mode ifd clen ilen} {
        set cname _zstream_[incr ::vfs::zseq]
        zlib s$mode $cname
        fconfigure $ifd -translation binary
        set cmd [list ::vfs::zstream_handler $cname $ifd $clen $ilen s$mode]
        set fd [rechan $cmd 2]
        set ::vfs::_zstream_pos($fd) 0
        return $fd
    }
}

