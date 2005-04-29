#
# Simple install script for tclodbc package
# (c) Roy Nurmi 1998-2001
#

proc output msg {
    #use messagebox when run in wish, otherways puts msgs to stdout
    if {[catch {tk_messageBox -message $msg}]} {puts $msg}
}

set tclodbc_version 2.5

set home [file dirname [info script]] 

if {![info exists tcl_pkgPath]} {
    # tcl_pkgPath not set, try to find lib directorey in auto_path (ok, kludge ;-)
    foreach i $auto_path {
	if [string match *lib $i] {
	    lappend tcl_pkgPath $i
	    break
	}
    }
    # if lib not found, take first in auto_path
    if {![info exists tcl_pkgPath]} {
	lappend tcl_pkgPath [lindex $auto_path 0]
    }
}

if {[file isdirectory [lindex $tcl_pkgPath 0]]} {
    set pkgDir [lindex $tcl_pkgPath 0]/tclodbc$tclodbc_version
    output "Installing tclodbc $tclodbc_version for Tcl $tcl_version to directory $pkgDir"

    # create root directory
    if {![file isdirectory $pkgDir]} {
	file mkdir $pkgDir
    }

    # copy the right version of the dll
    if {$tcl_version > "8.1"} {
	set dll_version 8.1
    } else {
	set dll_version $tcl_version 
    }
    file copy -force [file join $home tcl${dll_version}/tclodbc.dll] \
	    $pkgDir/tclodbc$tclodbc_version.dll

    # copy supporting tcl files
    set tclfiles [glob -nocomplain $home/tclutils/*]
    foreach i $tclfiles {
	file copy -force $i $pkgDir
    }

    # create subdirectories
    if {![file isdirectory [file join $pkgDir doc]]} {
	file mkdir [file join $pkgDir doc]
    }
    if {![file isdirectory [file join $pkgDir samples]]} {
	file mkdir [file join $pkgDir samples]
    }

    # copy documentation
    set docs [glob -nocomplain $home/doc/*]
    foreach i $docs {
	file copy -force $i [file join $pkgDir doc]
    }

    # copy samples
    set samples [glob -nocomplain $home/samples/*]
    foreach i $samples {
	file copy -force $i [file join $pkgDir samples]
    }

    # copy license file
    file copy -force [file join $home license.txt] $pkgDir   

    output "Installation successful."
} else {
    output "Can't install, check proper tcl installation first."
}

exit
