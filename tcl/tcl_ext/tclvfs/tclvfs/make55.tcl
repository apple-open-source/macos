#!/bin/sh
# The next line is executed by /bin/sh, but not tcl \
exec /usr/local/bin/tclsh8.4 $0 ${1+"$@"}


proc platform {} {
    global tcl_platform
    set plat [lindex $tcl_platform(os) 0]
    set mach $tcl_platform(machine)
    switch -glob -- $mach {
	sun4* { set mach sparc }
	intel -
	i*86* { set mach x86 }
	"Power Macintosh" { set mach ppc }
    }
    return "$plat-$mach"
}

proc makepackagedirs {pkgname} {
    file delete -force $pkgname
    file mkdir $pkgname
    file mkdir [file join $pkgname tcl]
    file mkdir [file join $pkgname doc]
    file mkdir [file join $pkgname examples]
    file mkdir [file join $pkgname [platform]]
    file mkdir [file join $pkgname tcl]
}

proc makepackage {pkgname} {
    global files
    makepackagedirs $pkgname

    foreach type [array names files] {
	foreach pat $files($type) {
	    foreach f [glob -nocomplain $pat] {
		file copy $f [file join $pkgname $type]
	    }
	}
    }
    file copy DESCRIPTION.txt $pkgname

    if {![catch {package require installer}]} {
	installer::mkIndex $pkgname
    }
} 


array set files {
    tcl         library/*.tcl
    examples    examples/*.tcl
    doc         {doc/*.n Readme.txt}
}
## how should files([platform]) be set?
## the version number ought to be a param, needs to come fro
## the config file: vfs_LIB_FILE

if [catch {open config.status} config] {
    error $config
}

while {[gets $config line] != -1} {
    regexp -expanded {s(.)@vfs_LIB_FILE@\1(.*)\1} $line => sep files([platform])
}
close $config

parray files


makepackage vfs1.2.1
