# nmea.tcl --
#
# NMEA protocol implementation
#
# Copyright (c) 2006-2007 Aaron Faupell
#
# RCS: @(#) $Id: nmea.tcl,v 1.4 2007/11/16 03:42:24 afaupell Exp $

package require Tcl 8.2
package provide nmea 0.2.0

namespace eval ::nmea {
    set ::nmea::nmea(checksum) 1
    set ::nmea::nmea(log) ""
}

proc ::nmea::open_port {port {speed 4800}} {
    variable nmea
    set nmea(fh) [open $port]
    fconfigure $nmea(fh) -mode $speed,n,8,1 -handshake xonxoff -buffering line -translation crlf
    fileevent $nmea(fh) readable [list ::nmea::read_port $nmea(fh)]
    return 1
}

proc ::nmea::open_file {file rate} {
    variable nmea
    set nmea(fh) [open $file]
    set nmea(rate) $rate
    fconfigure $nmea(fh) -buffering line -blocking 0 -translation auto
    if {$rate > 0} {
        after $rate [list ::nmea::read_file $nmea(fh)]
    }
}

proc ::nmea::configure_port {settings} {
    variable nmea
    fconfigure $nmea(fh) -mode $settings
}

proc ::nmea::close_port {} {
    variable nmea
    catch {close $nmea(fh)}
}

proc ::nmea::close_file {} {
    variable nmea
    catch {close $nmea(fh)}
}

proc ::nmea::read_port {f} {
    set line [gets $f]
    if {$::nmea::nmea(log) != ""} {
        puts $::nmea::nmea(log) $line
    }
    ::nmea::parse_nmea $line
}

proc ::nmea::read_file {f} {
    variable nmea
    if {![eof $f]} {
        set line [gets $f]
        if {[string match {$*} $line]} {
            ::nmea::parse_nmea $line
        } else {
            ::nmea::parse_nmea \$$line
        }
    }
    after $nmea(rate) [list ::nmea::read_file $f]
}

proc ::nmea::do_line {} {
    variable nmea
    if {![eof $nmea(fh)]} {
        set line [gets $nmea(fh)]
        if {[string match {$*} $line]} {
            ::nmea::parse_nmea $line
        } else {
            ::nmea::parse_nmea \$$line
        }
        return 1
    }
    return 0
}

proc ::nmea::input {sentence} {
    if {![string match "*,*" $sentence]} { set sentence [join $sentence ,] }
    if {[string match {$*} $sentence]} {
        ::nmea::parse_nmea $sentence
    } else {
        ::nmea::parse_nmea \$$sentence
    }
}

proc ::nmea::log {file} {
    variable nmea
    if {$file != ""} {
        if {$nmea(log) != ""} { error "already logging to a file" }
        set nmea(log) [open $file a]
    } else {
        catch {close $nmea(log)}
        set nmea(log) ""
    }
}

proc ::nmea::parse_nmea {line} {
    set line [split $line \$*]
    set cksum [lindex $line 2]
    set line [lindex $line 1]
    if {$cksum == "" || !$::nmea::nmea(checksum) || [checksum $line] == $cksum} {
        set line [split $line ,]
        set sentence [lindex $line 0]
        set line [lrange $line 1 end]
        if {[info commands ::nmea::$sentence] != ""} {
            $sentence $line
        }
    }
}

proc ::nmea::checksum {line} {
    set sum 0
    binary scan $line c* line
    foreach char $line {
        set sum [expr {$sum ^ ($char % 128)}]
    }
    return [format %02X [expr {$sum % 256}]]
}

proc ::nmea::write {type args} {
    variable nmea
    set data $type,[join $args ,]
    puts $nmea(fh) \$$data*[checksum $data]
}
