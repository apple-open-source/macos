#!/bin/sh
# The next line restarts using wish \
exec wish $0 ${1+"$@"}
#
# Initialization of some global variables

eval destroy [winfo children .]

switch -glob [info sharedlibextension] {
    .so* {
	set libs [list libpng.so.2.1.0 libjpeg.so.62.0.0 libtiff.so.3.4.37 \
		libz.so.1.1.3 libttf.so.1.2.0 libungif.so.3.1.0]
    }
    ..a {
	set libs [list libpng.a libjpeg.a libtiff.a libz.a libttf.a libungif.a]
    }
    .dll {
	set libs [list png.lib jpeg62.lib tiff.lib zlib.lib ttf.lib ungif.lib]
	set dll [list png.dll jpeg62.dll tiff.dll zlib.dll ttf.dll ungif.dll]
    }
    * {
	set libs [list libpng[info sharedlibextension] libjpeg[info sharedlibextension] \
		libtiff[info sharedlibextension] libz[info sharedlibextension] \
		libttf[info sharedlibextension] libungif[info sharedlibextension]]
    }
}

if [info exists env(PATH)] {
    if [string compare $tcl_platform(platform) windows] {
	set dirs [split $env(PATH) :]
    } else {
	set dirs [split $env(PATH) \;]
    }
} else {
    set dirs "/usr/local/lib /usr/lib /lib"
}

foreach dir "$dirs C:/WINDOWS/* C:/WINNT/*" {
   foreach d [list $dir [file join [file dirname $dir] lib]] {
	set x [glob -nocomplain [file join $d \{lib,\}tcl\[78\]*[info sharedlibextension]*]]
	if [string compare $x {}] break
    }
    if [string compare $x {}] break
}


label .f1 -text "Where should the following files be installed?"
pack .f1
proc line {f label default} {
    frame $f
    label $f.l -text $label
    entry $f.e -width 50
    $f.e insert end $default
    pack $f.l -side left
    pack $f.e -side right
    pack $f -expand y -fill both
}
set prefix [file dirname [file dirname $tk_library]]

if [string compare $tcl_platform(platform) windows] {
    set imglibs [lindex [file split $x] end]
    if [string match libtcl?.?[info sharedlibextension]* $imglibs] {
	set imglibs libimg1.2[info sharedlibextension]
    } else {
	set imglibs libimg12[info sharedlibextension]
    }
} else {
    set x [lindex $x 0]
    set systemdll [file dirname $x]
    set imglibs [list img1280.dll img1281.dll]
    line .f2 "system dll's" $systemdll
}
line .f3 "system libraries"  [file join $prefix lib]
line .f4 "system headers" [file join $prefix include]
line .f5 "Img 1.2 files" [file join $prefix lib Img1.2]
frame .f6
button .f6.install -text Install -command Install
button .f6.exit -text Exit -command "destroy ."
pack .f6.install .f6.exit -side left -fill both -expand y
pack .f6 -fill both -expand y

proc Copy {src dst} {
    if [file exists $src] {
	file delete -force [file join $dst $src]
	puts_stdout "copying $src to $dst"
	file copy $src $dst
	return 1
    }
    return 0
}

proc Install {} {
    global libs dll tcl_platform imglibs
    if [winfo exists .t] {
	raise .t
    } else {
	toplevel .t
	frame .t.f
	button .t.f.d -text dismiss -command [list destroy .t]
	pack .t.f.d -side left
	pack .t.f -side top -fill x
	text .t.t -yscrollcommand [list .t.s set]
	scrollbar .t.s -command [list .t.t yview]
	pack .t.t .t.s -side left -expand y -fill both
    }
    .t.t delete 1.0 end
    proc puts_stdout args {
	.t.t insert end "[lindex $args 0]\n"
	.t.t see end
	update
    }
    .t.t see end
    if ![string compare $tcl_platform(platform) windows] {
	set dir [.f2.e get]
	foreach lib $dll {
	    Copy $lib $dir
	}
    }
    set dir [.f3.e get]
    foreach lib $libs {
	if {[Copy $lib $dir] && ![string compare [info sharedlibextension] .so]} {
	    while {[string compare .so [set ext [file extension $lib]]]} {
		file delete [set file [file join $dir [file rootname $lib]]]
		puts_stdout "ln -s $lib $file"
		exec ln -s $lib $file
		set lib [file rootname $lib]
	    }
	}
    }
    set dir [.f4.e get]
    foreach lib [list zlib.h zconf.h png.h pngconf.h jpeglib.h jconfig.h \
		jmorecfg.h jerror.h tiff.h tiffio.h tiffconf.h freetype.h gif_lib.h] {
	Copy $lib $dir
    }
    set dir [.f5.e get]
    catch {file mkdir $dir}
    foreach lib "$imglibs pkgIndex.tcl" {
	Copy $lib $dir
    }
    puts_stdout "---------- installation complete ----------"
}
