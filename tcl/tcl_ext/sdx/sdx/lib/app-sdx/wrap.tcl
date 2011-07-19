# wrap - convert file system tree to a starkit
# by Matt Newman <matt@sensus.org> and Jean-Claude Wippler <jcw@equi4.com>
#
# 20020607 jcw	added -writeable, switched to starkit package
#		the default is now to creates a read-only starkit

set header \
{#!/bin/sh
# %
exec @PROG@ "$0" ${1+"$@"}
package require starkit
starkit::header @TYPE@ @OPTS@
}
append header \32
regsub % $header \\ header

if {[llength $argv] < 1} {
  puts stderr "Usage: $argv0 outfile ?-option ...?"
  exit 1
}

set origtime [clock seconds]

proc readfile {name} {
  set fd [open $name]
  fconfigure $fd -translation binary
  set data [read $fd]
  close $fd
  return $data
}

proc writefile {name data} {
  set fd [open $name w]
  fconfigure $fd -translation binary
  puts -nonewline $fd $data
  close $fd
}

# decode Windows .ICO file contents into the individual bit maps
# Each icon header has:
#  byte : width in pixels (0 for 256 px)
#  byte : height in pixels (0 for 256px)
#  byte : number of colors in palette (0 for truecolor)
#  byte : reserved (0)
#  short: number of color planes (0 or 1)
#  short: number of bits per pixel
#  long : size of the bitmap data in bytes
#  long : offset to the start of bitmap data
proc decICO {dat} {
  set result {}
  binary scan $dat sss - type count
  for {set pos 6} {[incr count -1] >= 0} {incr pos 16} {
    binary scan $dat @${pos}ccccssii w h cc - p bc bir io
    if {$cc == 0} { set cc 256 }
    #puts "pos $pos w $w h $h cc $cc p $p bc $bc bir $bir io $io"
    binary scan $dat @${io}a$bir image
    lappend result ${w}x${h}/$bc $image
  }
  return $result
}

proc LoadHeader {filename} {
  if {[file normalize $filename] == [info nameofexe]} {
    puts stderr "file in use, cannot be prefix: [file normalize $filename]"
    exit 1
  }
  set size [file size $filename]
  catch {
    package require vfs::mk4
    vfs::mk4::Mount $filename hdr -readonly
    # we only look for an icon if the runtime is called *.exe (!)
    if {[string tolower [file extension $filename]] == ".exe"} {
      catch { set ::origicon [readfile hdr/tclkit.ico] }
    }
  }
  catch { vfs::unmount $filename }
  return [readfile $filename]
}

set out [lindex $argv 0]
set base [file root [file tail $out]]
set idir $base.vfs
if {![file isdirectory $idir]} {
    if {[file isdirectory $base] && [file exists $base/main.tcl]} {
	set idir $base
	if {[lindex [split $out .] 0] eq $out} {
	    # append kit if user hasn't specified an extension
	    set out $base.kit
	}
    }
}
set compress 1
set verbose 0
set ropts -readonly
set prefix 0
set reusefile 0
set prog tclsh
set type mk4
set explist {}
set syncopts {}

set a [lindex $argv 1]
while {[string match -* $a]} {
  switch -- $a {
    -interp {
      set prog [lindex $argv 2]
      set argv [lreplace $argv 1 2]
    }
    -runtime {
      set pfile [lindex $argv 2]
      if {$pfile == $out} {
      	set reusefile 1
      } else {
      	set header [LoadHeader $pfile]
      }
      set argv [lreplace $argv 1 2]
      set prefix 1
    }
    -writable -
    -writeable {
      #set ropts "-nocommit"
      set ropts ""
      set argv [lreplace $argv 1 1]
    }
    -nocomp -
    -nocompress {
      set compress 0
      set argv [lreplace $argv 1 1]
    }
    -verbose {
      set verbose 1
      set argv [lreplace $argv 1 1]
    }
    -uncomp {
      lappend explist [lindex $argv 2]
      set argv [lreplace $argv 1 2]
    }
    -vfs {
      set idir [lindex $argv 2]
      set argv [lreplace $argv 1 2]
    }
    default {
      lappend syncopts [lindex $argv 1] [lindex $argv 2]
      set argv [lreplace $argv 1 2]
    }
  }
  set a [lindex $argv 1]
}

if {![file isdir $idir]} {
  puts stderr "Input directory not found: $idir"
  exit 1
}

if {!$prefix} {
  regsub @PROG@ $header $prog header
  regsub @OPTS@ $header $ropts header
  regsub @TYPE@ $header $type header

  set n [string length $header]
  while {$n <= 240} {
    append header ################
    incr n 16
  }

  set slop [expr { 15 - (($n + 15) % 16) }]
  for {set i 0} {$i < $slop} {incr i} {
    append header #
  }
  set n [string length $header]
  if {$n % 16 != 0} {
    error "Header size is $n, should be a multiple of 16"
  }
}

if {!$reusefile} {
  writefile $out $header
}

set origsize [file size $out]

switch $tcl_platform(platform) {
  unix {
    catch {file attributes $out -permissions +x}
  }
  windows {
    set batfile [file root $out].bat
    # 2005-03-18 don't create a batfile if "-runtime" is specified
    if {![file exists $batfile] && ![info exists pfile]} {
      set fd [open $batfile w]
      puts -nonewline $fd \
	  "@$prog [file tail $out] %1 %2 %3 %4 %5 %6 %7 %8 %9"
      close $fd
    }
  }
  macintosh {
    catch {file attributes $out -creator TKd4}
  }
}

# 2003-02-08: added code to patch icon in windows executable
# triggered by existence of tclkit.ico in vfs dir *and* tclkit.ico in orig

# careful: this applies only to windows executables, but the
# icon replacement can in fact take place on any platform...

if {[info exists origicon] && [file exists [file join $idir tclkit.ico]]} {
  puts " customizing tclkit.ico in executable"
  set custicon [readfile [file join $idir tclkit.ico]]
  array set newimg [decICO $custicon]
  foreach {k v} [decICO $origicon] {
    if {[info exists newimg($k)]} {
      set len [string length $v]
      set pos [string first $v $header]
      if {$pos < 0} {
      	puts "  icon $k: NOT FOUND"
      } elseif {[string length $newimg($k)] != $len} {
      	puts "  icon $k: NOT SAME SIZE"
      } else {
      	binary scan $header a${pos}a${len}a* prefix - suffix
      	set header "$prefix$newimg($k)$suffix"
      	puts "  icon $k: replaced"
      }
    }
  }
  writefile $out $header
}

# 2005-03-15 added AF's code to customize version/description strings in exe's
if {[info exists pfile] && 
    [string tolower [file extension $pfile]] == ".exe" &&
    [file exists [file join $idir tclkit.inf]]} {
  puts " customizing strings in executable"
  package require stringfileinfo
  set fd [open [file join $idir tclkit.inf]]
  array set strinfo [read $fd]
  close $fd
  ::stringfileinfo::writeStringInfo $out strinfo
}

proc tclLog {msg} { puts $msg }

proc mkFileStart {filename} {
  set end [file size $filename]
  if {$end < 27} {
    fail "file too small, cannot be a datafile"
  }

  set fd [open $filename]
  fconfigure $fd -translation binary
  seek $fd -16 end
  binary scan [read $fd 16] IIII a b c d

 #puts [format %x-%d-%x-%d $a $b $c $d]

  if {($c >> 24) != -128} {
    error "this is not a Metakit datafile"
  }

  # avoid negative sign / overflow issues
  if {[format %x [expr {$a & 0xffffffff}]] eq "80000000"} {
    set start [expr {$end - 16 - $b}]
  } else {
    # if the file is in commit-progress state, we need to do more
    error "this code needs to be finished..."
  }
  
  return $start
}

if {![catch { package require Mk4tcl }]} {
  vfs::mk4::Mount $out $out
  set argv $syncopts
  lappend argv -compress $compress -verbose $verbose -noerror 0 $idir $out

  source [file join [file dirname [info script]] sync.tcl] 

    # 2003-06-19: new "-uncomp name" option to store specific file(s)
    #		    in uncompressed form, even if the rest is compressed
  set o $vfs::mk4::compress
  set vfs::mk4::compress 0
  foreach f $explist {
    file delete -force [file join $out $f]
    file copy [file join $idir $f] [file join $out $f]
  }
  set vfs::mk4::compress $o

  vfs::unmount $out
} elseif {![catch { package require vlerq }]} {
  package require vfs::m2m 1.8

  if {$verbose} {
    puts "Mk4tcl not found, switching to the Vlerq driver."
  }

  # 2006-12-06, must do a bit more work for starpacks when there is no Mk4tcl:
  #   - split the tail off
  #   - truncate the file so only the head remains
  #   - copy the original vfs contents to m2m, to be overwritten/extended later
  
  set outsize [file size $out]
  
  if {![catch { mkFileStart $out } mkpos]} {
    set fd [open $out]
    fconfigure $fd -translation binary
    set outhead [read $fd $mkpos]
    set origvfs [read $fd]
    close $fd

    set fd [open $out w]
    fconfigure $fd -translation binary
    puts -nonewline $fd $outhead
    close $fd
  }
  
  vfs::m2m::Mount $out $out

  if {[info exists origvfs]} {
    set fd [open $out.tmp w]
    fconfigure $fd -translation binary
    puts -nonewline $fd $origvfs
    close $fd
    
    package require vfs::mkcl
    vfs::mkcl::Mount $out.tmp $out.tmp
    
    set argv [list -verbose 0 $out.tmp $out]
    source [file join [file dirname [info script]] sync.tcl] 

    vfs::unmount $out.tmp
    file delete $out.tmp
  }
  
  set argv $syncopts
  lappend argv -verbose $verbose -noerror 0 $idir $out

  source [file join [file dirname [info script]] sync.tcl] 

  vfs::unmount $out
} else {
  puts stderr "cannot find required packages (Mk4tcl or Vlerq)"
  exit 1
}
