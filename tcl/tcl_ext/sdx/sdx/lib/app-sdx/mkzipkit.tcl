# mkzipkit.tcl -
#
#	Convert a zip archive into a Tcl Module or zipkit file
#	by adding a SFX header that can enable TclKit to mount
#	the archive. This provides an alternative to Metakit-based
#	starkits.
#
# Copyright (c) 2004 Pascal Scheffers
# Copyright (c) 2006-2008 Pat Thoyts <patthoyts@users.sourceforge.net>

# The default module prefix
variable SFX_STUB [format {#!/bin/sh
# %c
exec tclsh "$0" ${1+"$@"}
# This is a zip-based Tcl Module
package require vfs::zip
vfs::zip::Mount [info script] [info script]
if {[file exists [file join [info script] main.tcl]]} {
    source [file join [info script] main.tcl]
}
} 0x5C]

# mkzipkit --
#
#	Prefixes the specified zip archive with the tclmodule mount stub
#	and writes out to outfile
#
proc mkzipkit { zipfile outfile {stubfile {}}} {
    variable SFX_STUB
    if {$stubfile eq {}} {
        set stub $SFX_STUB
    } else {
        set f [open $stubfile r]
        fconfigure $f -translation binary -encoding binary -eofchar {}
        set stub [read $f]
        close $f
    }
    append stub \x1A
    make_sfx $zipfile $outfile $stub
}

# make_sfx --
#
#	Adds an arbitrary 'sfx' to a zip file, and adjusts the central
#	directory and file items to compensate for this extra data.
#
proc make_sfx { zipfile outfile sfx_stub } {
    
    set in [open $zipfile r]
    fconfigure $in -translation binary -encoding binary
    
    set out [open $outfile w+]
    fconfigure $out -translation binary -encoding binary
    
    puts -nonewline $out $sfx_stub
    
    set offset [tell $out]
    
    lappend report "sfx stub size: $offset"
    
    fcopy $in $out

    set size [tell $out]
    
    # Now seek in $out to find the end of directory signature:
    # The structure itself is 24 bytes long, followed by a maximum of
    # 64Kbytes text
    
    if { $size < 65559 } {
        set seek 0
    } else {
        set seek [expr { $size - 65559 } ]
    }
    #flush $out
    seek $out $seek
    #puts "$seek [tell $out]"
    
    set data [read $out]
    set start_of_end [string last "\x50\x4b\x05\x06" $data]
    
    set start_of_end [expr {$start_of_end + $seek}]
    lappend report "SEO: $start_of_end ([expr {$start_of_end-$size}])\
        [string length $data]"
    
    seek $out $start_of_end
    set end_of_ctrl_dir [read $out]
    
    binary scan $end_of_ctrl_dir issssiis \
        eocd(signature) eocd(disknbr) eocd(ctrldirdisk) \
        eocd(numondisk) eocd(totalnum) eocd(dirsize) eocd(diroffset) \
        eocd(comment_len)
    
    lappend report "End of central directory: [array get eocd]"
    
    seek $out [expr {$start_of_end+16}]
    
    #adjust offset of start of central directory by the length of our sfx stub
    puts -nonewline $out [binary format i [expr {$eocd(diroffset)+$offset}]]
    flush $out
    
    seek $out $start_of_end
    set end_of_ctrl_dir [read $out]
    binary scan $end_of_ctrl_dir issssiis \
        eocd(signature) eocd(disknbr) eocd(ctrldirdisk) \
        eocd(numondisk) eocd(totalnum) eocd(dirsize) eocd(diroffset) \
        eocd(comment_len)
    
    lappend report "New dir offset: $eocd(diroffset)"
    lappend report "Adjusting $eocd(totalnum) zip file items."
    
    seek $out $eocd(diroffset)
    for {set i 0} {$i <$eocd(totalnum)} {incr i} {
        set current_file [tell $out]
        set fileheader [read $out 46]
        binary scan $fileheader is2sss2ii2s3ssii \
            x(sig) x(version) x(flags) x(method) \
            x(date) x(crc32) x(sizes) x(lengths) \
            x(diskno) x(iattr) x(eattr) x(offset)
        
        if { $x(sig) != 33639248 } {
            error "Bad file header signature at item $i: $x(sig)"
        }
        
        foreach size $x(lengths) var {filename extrafield comment} {
            if { $size > 0 } {
                set x($var) [read $out $size]
            } else {
                set x($var) ""
            }
        }
        set next_file [tell $out]
        lappend report "file $i: $x(offset) $x(sizes) $x(filename)"
        
        seek $out [expr {$current_file+42}]
        puts -nonewline $out [binary format i [expr {$x(offset)+$offset}]]
        
        # verify:
        flush $out
        seek $out $current_file
        set fileheader [read $out 46]
        lappend report "old $x(offset) + $offset"
        binary scan $fileheader is2sss2ii2s3ssii \
            x(sig) x(version) x(flags) x(method) \
            x(date) x(crc32) x(sizes) x(lengths) \
            x(diskno) x(iattr) x(eattr) x(offset)
        lappend report "new $x(offset)"
        
        seek $out $next_file
    }
    #puts [join $report \n]
}

if {[llength $argv] < 2} {
    puts stderr "usage: $argv0 inputfile outputfile ?stubfile?"
    exit 1
}

eval [linsert $argv 0 mkzipkit]
