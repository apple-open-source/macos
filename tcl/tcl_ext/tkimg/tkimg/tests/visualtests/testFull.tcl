package require Tk

proc initPackages { args } {
    global gPkg

    foreach pkg $args {
	set retVal [catch {package require $pkg} gPkg(ext,$pkg,version)]
	set gPkg(ext,$pkg,avail) [expr !$retVal]
    }
}

initPackages img::bmp  img::gif  img::ico  img::jpeg img::pcx \
             img::png  img::ppm  img::raw  img::sgi  img::sun \
             img::tga  img::tiff img::xbm  img::xpm  img::window

set retVal [catch {package require Img} version]
if { $retVal } {
    error "Trying to load package Img: $version"
}

cd [file dirname [info script]]

source [file join "utils" "testUtil.tcl"]
source [file join "utils" "testGUI.tcl"]
# We get the global variable ui_enable_tk from above Tcl module.

source [file join "utils" "testImgs.tcl"]
source [file join "utils" "testReadWrite.tcl"]

if { $argc != 1 } {
    set testMode [expr $modeFile | $modeBin | $modeUU]
} else {
    set testMode [lindex $argv 0]
}

PH "Image Read/Write (Full Images)"

P "This test tries to store the content of a canvas window in image files"
P "using all file formats available in the tkImg package."
P "After writing we try to read the image back into a photo by using the"
P "auto-detect mechanism of tkImg. If that fails, we use the \"-format\" option."
P ""
if { $ui_enable_tk } {
    P "Set the environment variable UI_TK to 0 before running this test,"
    P "to run this test in batch mode without displaying the images."
    P ""
}

if { $tcl_platform(platform) eq "windows" && $ui_enable_tk } {
    catch { console show }
}

ui_init "testFull.tcl: Read/Write (Full Images)" "+320+30"
SetFileTypes

drawTestCanvas $version

P ""
set sep ""
if { $ui_enable_tk } {
    set sep "\n\t"
}
set count 1
foreach elem $fmtList {
    set ext [lindex $elem 0]
    set fmt [lindex $elem 1]
    set opt [lindex $elem 2]
    catch { file mkdir out }
    set fname [file join out testFull$ext]
    set msg "Image $count: $fname Format: $fmt $sep (Options: $opt)"
    P $msg

    PN "\t"
    set ph [getCanvasPhoto .t.c]
    writePhotoFile $ph $fname "$fmt $opt" 1
    if { $testMode & $modeFile } {
	set ph [readPhotoFile1 $fname "$fmt $opt"]
	if { $ph eq "" } {
	    set ph [createErrImg]
	}
	set msg "Image $count.1: $fname Format: $fmt $sep (Read from file 1)"
	ui_addphoto $ph $msg

	set ph [readPhotoFile2 $fname "$fmt $opt" -1 -1]
	if { $ph eq "" } {
	    set ph [createErrImg]
	}
	set msg "Image $count.2: $fname Format: $fmt $sep (Read from file 2)"
	ui_addphoto $ph $msg
    }
    if { $testMode & $modeBin } {
	set ph [readPhotoBinary1 $fname "$fmt $opt"]
	if { $ph eq "" } {
	    set ph [createErrImg]
	}
	set msg "Image $count.3: $fname Format: $fmt $sep (Read as binary 1)"
	ui_addphoto $ph $msg

	set ph [readPhotoBinary2 $fname "$fmt $opt" -1 -1]
	if { $ph eq "" } {
	    set ph [createErrImg]
	}
	set msg "Image $count.4: $fname Format: $fmt $sep (Read as binary 2)"
	ui_addphoto $ph $msg
    }
    if { $testMode & $modeUU } {
	set ph [getCanvasPhoto .t.c]
	set str [writePhotoString $ph "$fmt $opt" 1]
	if { $str eq "" } {
	    set ph [createErrImg]
	} else {
	    set ph [readPhotoString $str "$fmt $opt" -1 -1]
	    if { $ph eq "" } {
		set ph [createErrImg]
	    }
	}
	set msg "Image $count.5: $fname Format: $fmt $sep (Read as uuencoded string)"
	ui_addphoto $ph $msg
    }

    P ""
    incr count
}

PS
P "End of test"

P ""
P "Package tkImg: $version"
ui_show
