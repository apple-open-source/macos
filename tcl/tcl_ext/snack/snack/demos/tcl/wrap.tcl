#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

# This utility creates stand-alone executables from Snack Tcl scripts

if {[string equal $tcl_platform(os) "Linux"]} {
 set platform linux
}
if {[string equal $tcl_platform(platform) "windows"]} {
 set platform windows
}

if {[info exists argv] == 0} { set argv "" }
set appname [file rootname [lindex $argv 0]]
if {$appname == ""} {
 pack [label .l1 -text "This utility creates\nstand-alone executables"]
 update
 set appname [file rootname [lindex [file split \
         [tk_getOpenFile -filetypes {{{Tcl scripts} {.tcl}}}]] end]]
  if {$appname == ""} return
}

set outdir [pwd]

if {[string equal $platform "linux"]} {
 set tclkit  tclkit-linux-x86
 set runtime tclkit-linux-x86
 set shlibext .so
 set binext ""
 set target linux
 set wrapdir /tmp
} elseif {[string equal $platform "windows"]} {
# set tclkit  tclkit-win32-sh.upx.exe
 set tclkit  tclkit-win32-sh.exe
 set runtime tclkit-win32.exe
 set shlibext .dll
 set binext .exe
 set target windows
 if {[info exists ::env(TEMP)] && $::env(TEMP) != ""} {
  set wrapdir $::env(TEMP)
 } elseif {[info exists ::env(TMP)] && $::env(TMP) != ""} {
  set wrapdir $::env(TMP)
 } else {
  set wrapdir ""
 }
}

set f1 [open $appname.tcl r]
set f2 [open $wrapdir/$appname.tcl w]
while {[eof $f1] == 0} {
 set line [gets $f1]
 if [string match {package require*snack*} $line] {
  set doSnack 1
  puts $f2 "catch {package require Tk}"
 }
 if [string match {package require*sound*} $line] {
  set doSound 1
 }
 if [string match {source *} $line] {
  set f3 [open [lindex [split $line] end] r]
  puts $f2 [read -nonewline $f3]
  close $f3
  continue
 }
 puts $f2 $line
}
close $f1
close $f2

if {[string equal $platform "linux"]} {
 file copy -force $tclkit  $wrapdir/tclkit
 file copy -force sdx      $wrapdir/
}
if {[string equal $platform "windows"]} {
 file copy -force $tclkit  $wrapdir/tclkitsh.exe
 file copy -force sdx.kit  $wrapdir/
 file copy -force sdx.bat  $wrapdir/
 file copy -force sdx      $wrapdir/
}


cd $wrapdir
file delete -force $appname.vfs
if {[string equal $platform "linux"]} {
 exec sdx qwrap  $appname.tcl
 exec sdx unwrap $appname.kit
} else {
 exec tclsh84 sdx qwrap  $appname.tcl
 exec tclsh84 sdx unwrap $appname.kit
}

foreach file [lrange $argv 1 end] {
 set dstfile [file join $appname.vfs/ $file]
 file copy $outdir/$file $dstfile
}


# Get the Snack package's directory and copy into $appname.vfs/lib/
cd $outdir
package require snack
set tmp [package ifneeded snack [package provide snack]]
set tmp [lindex [lindex [split $tmp ";"] 0] end]
set snackdir [file dirname $tmp]
set snackdirname [file tail [file dirname $tmp]]

file mkdir $wrapdir/$appname.vfs/lib/
file mkdir $wrapdir/$appname.vfs/lib/$snackdirname
if {[info exists doSnack]} {
 file copy $snackdir/libsnack$shlibext $wrapdir/$appname.vfs/lib/$snackdirname
} else {
 file copy $snackdir/libsound$shlibext $wrapdir/$appname.vfs/lib/$snackdirname
}
file copy $snackdir/snack.tcl    $wrapdir/$appname.vfs/lib/$snackdirname
file copy $snackdir/pkgIndex.tcl $wrapdir/$appname.vfs/lib/$snackdirname


# Do wrapping

cd $wrapdir
if {[string equal $platform "linux"]} {
 exec sdx wrap $appname -runtime $outdir/$runtime
} else {
 exec tclsh84 sdx wrap $appname -runtime $outdir/$runtime
}
file copy -force $appname $outdir/$appname$binext

exit
