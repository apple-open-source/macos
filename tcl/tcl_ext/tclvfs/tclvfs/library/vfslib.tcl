# Remnants of what used to be VFS init, this is TclKit-specific

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

# use rechan to define memchan and zstream if available
if {[info command rechan] != "" || ![catch {load "" rechan}]} {
    proc vfs::memchan_handler {cmd fd args} {
	upvar 1 ::vfs::_memchan_buf($fd) buf
	upvar 1 ::vfs::_memchan_pos($fd) pos
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
		unset buf pos
	    }
	    default { error "bad cmd in memchan_handler: $cmd" }
	}
    }

    proc vfs::memchan {} {
	set fd [rechan ::vfs::memchan_handler 6]
	set ::vfs::_memchan_buf($fd) ""
	set ::vfs::_memchan_pos($fd) 0
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
    proc vfs::zstream {mode ifd clen ilen} {
	set cname _zstream_[incr ::vfs::zseq]
	zlib s$mode $cname
	set cmd [list ::vfs::zstream_handler $cname $ifd $clen $ilen s$mode]
	set ifd [rechan $cmd 2]
	set ::vfs::_zstream_pos($fd) 0
	return $fd
    }
}

