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

PH "Image Read/Write (Different sizes)"

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

ui_init "testSmall.tcl: Read/Write (Different small sizes)" "+320+30"
SetFileTypes

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
    set prefix [file join out testSmall]

    P "Format $fmt :"
    for { set w 1 } { $w <=4 } { incr w } {
	for { set h 1 } { $h <=4 } { incr h } {
	    P "Creating a photo of size: $w x $h"
	    set ph [image create photo -width $w -height $h]
	    set imgData {}
	    for { set y 1 } { $y <= $h } { incr y } {
	        set imgLine {}
		for { set x 1 } { $x <= $w } { incr x } {
		    set col 0
		    if { $x % 2 == 1 && $y % 2 == 1 ||
		         $x % 2 == 0 && $y % 2 == 0 } {
			set col 255
		    }
		    set val [format "#%02x%02x%02x" $col $col $col]
                    lappend imgLine $val
                    if { $fmt eq "xbm" } {
                        $ph put -to [expr $x-1] [expr $y-1] $val
                        $ph transparency set [expr $x-1] [expr $y-1] [expr $col]
                    }
		}
	        lappend imgData $imgLine
	    }
	    set zoom 8
            if { $fmt ne "xbm" } {
                $ph put $imgData
            }
	    set fname [format "%s_w%d_h%d%s" $prefix $w $h $ext]
	    # Write the image to a file and read it back again.
	    writePhotoFile $ph $fname "$fmt $opt" 1
	    set ph [readPhotoFile1 $fname "$fmt $opt"]
	    if { $ph == {} } {
		set ph [createErrImg]
		set zoom 1
	    }
	    # Write the image to a uuencoded string and read it back again.
	    set str [writePhotoString $ph "$fmt $opt" 1]
	    if { $str == "" } {
		set ph [createErrImg]
		set zoom 1
	    } else {
		set ph [readPhotoString $str "$fmt $opt" -1 -1]
		if { $ph == {} } {
		    set ph [createErrImg]
		    set zoom 1
		}
	    }
	    # Write the image to a uuencoded string and read it back again.
	    set zw [expr [image width  $ph] * $zoom]
	    set zh [expr [image height $ph] * $zoom]
	    set zoomPh [image create photo -width $zw -height $zh]
	    $zoomPh copy $ph -zoom $zoom $zoom
	    image delete $ph
	    set msg "Image: $fname Format: $fmt $sep (Width: $w Height: $h)"
	    ui_addphoto $zoomPh $msg
	    P ""
	}
    }

    P ""
    incr count
}

PS
P "End of test"

P ""
P "Package tkImg: $version"
ui_show
