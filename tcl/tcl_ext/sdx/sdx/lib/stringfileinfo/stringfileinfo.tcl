# 2004-07-24 v0.1   original code by Aaron Faupell
# 2005-03-15 v0.2   minor tweaks by jcw to display progress/failures

package provide stringfileinfo 0.2

namespace eval ::stringfileinfo {}

proc ::stringfileinfo::getdword {fh} {
    binary scan [read $fh 4] i* tmp
    return $tmp
}

proc ::stringfileinfo::getword {fh} {
    binary scan [read $fh 2] s* tmp
    return $tmp
}

proc ::stringfileinfo::getStringInfo {file array} {
    upvar $array ret
    array set ret {}
    set fh [open $file r]
    if {[read $fh 2] == "MZ" && [seek $fh 24 start] == "" && [scan [read $fh 1] %c] >= 64} {
        seek $fh 60 start
        seek $fh [getword $fh] start
        if {[read $fh 4] == "PE\000\000"} {
            seek $fh 24 current
            seek $fh [getdword $fh] start
        }
    }
    fconfigure $fh -translation lf -encoding unicode -eofchar {}
    set data [read $fh]
    close $fh

    if {[set s [string first "StringFileInfo\000" $data]] < 0} {error "no string information found"}
    incr s -3

    if {![regexp {(.)\000(.)StringFileInfo\000(.)\000(.)(....)(....)\000} [string range $data $s end] --> len type len2 type2 lang code]} {
        error "string information corrupt"
    }
    array set ret [list Language $lang CodePage $code]
    set len [expr [scan $len %c] / 2]
    set len2 [expr [scan $len2 %c] / 2]
    set data [string range $data $s [expr {$s + $len}]]
    set s 30
    while {$s < $len2} {
        scan [string range $data $s end] %c%c%c slen vlen type
        if {$slen == 0} return
        set slen [expr {$slen / 2}]
        set name [string range $data [expr {$s + 3}] [expr {$s + $slen - $vlen - 1}]]
        set value [string range $data [expr {$s + $slen - $vlen}] [expr {$s + $slen - 2}]]
        set s [expr {$s + $slen + ($slen % 2)}]
        set ret([string trimright $name \000]) $value
    }
}

# convert to utf-16, fix byte order on big-endian machines, jcw 2005-03-18
proc utf16le {s} {
  set s [encoding convertto unicode $s]
  if {$::tcl_platform(byteOrder) eq "bigEndian"} {
    binary scan $s S* t
    set s [binary format s* $t]
  }
  return $s
}

# modifies array passed as 2nd arg
proc ::stringfileinfo::writeStringInfo {file array} {
    upvar $array val
    set fh [open $file r+]
    fconfigure $fh -translation binary
    set data [read $fh]
    set s [string first [utf16le "StringFileInfo\0"] $data]
    if {$s < 0} {
      close $fh
      # this error message does not belong in the stringfileinfo package :(
      puts " tclkit.inf ignored, no version resource found"
      return
    }
    if {![info exists val(CodePage)]} { set val(CodePage) 04b0 }
    if {![info exists val(Language)]} { set val(Language) 0409 }
    
    binary scan $data "x[incr s -6]s" len
    seek $fh [incr s 36] start
    incr len -36

    puts -nonewline $fh [binary format sss $len 0 1]
    puts -nonewline $fh [utf16le "$val(Language)$val(CodePage)\0"]
    unset val(CodePage) val(Language)

    set len [expr {$len / 2 - 12}]
    foreach x [array names val] {
        set vlen [expr {[string length $val($x)] + 1}]
        set nlen [string length $x]
        set npad [expr {$nlen % 2}]
        set tlen [expr {$vlen + $nlen + $npad + 4}]
        set tpad [expr {$tlen % 2}]

	#set o [tell $fh]
	#puts [format "\noff %x len %d end %x" $o $len [expr {$o+2*$len}]]
        #puts "total length: [expr {$tlen * 2}] bytes"
        #puts "value length $val($x) $vlen chars"
        #puts "name length: $x $nlen"
        #puts "value padding: $npad"
        #puts "total padding: $tpad"

        if {($tlen + $tpad) > $len} { puts "  $x: too long" ; break }
	puts "  $x: set to '$val($x)'"
        puts -nonewline $fh [binary format sss [expr {$tlen * 2}] $vlen 1]
	set snpad [string repeat \0 $npad]
	set stpad [string repeat \0 $tpad]
        puts -nonewline $fh [utf16le "$x\0$snpad$val($x)\0$stpad"]
        set len [expr {$len - $tlen - $tpad}]
    }
    puts -nonewline $fh [string repeat \0 [expr {$len * 2}]]
    close $fh
}
