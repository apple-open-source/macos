# Some constants
set modeFile 0x01
set modeBin  0x02
set modeUU   0x04
set modeFileStr "File IO"
set modeBinStr  "Binary IO"
set modeUUStr   "UUencoded IO"

# The list of file formats to be tested.
# First entry specifies the file extension used to create the image filenames.
# Second entry specifies the image format name as used by the Img extension.
# Third entry specifies optional format options.

set fmtList [list \
    	[list ".bmp"   "bmp"  ""] \
    	[list ".gif"   "gif"  ""] \
    	[list ".ico"   "ico"  ""] \
    	[list ".jpg"   "jpeg" ""] \
    	[list ".pcx"   "pcx"  ""] \
    	[list ".png"   "png"  ""] \
	[list ".ppm"   "ppm"  ""] \
    	[list ".raw"   "raw"  "-useheader true -nomap true -nchan 3"] \
    	[list ".rgb"   "sgi"  ""] \
    	[list ".ras"   "sun"  ""] \
    	[list ".tga"   "tga"  ""] \
    	[list ".tif"   "tiff" ""] \
    	[list ".xbm"   "xbm"  ""] \
    	[list ".xpm"   "xpm"  ""] ]


# Load image data directly from a file into a photo image.
# Uses commands: image create photo -file "fileName"
proc readPhotoFile1 { name fmt } {
    PN "File read 1: "

    set sTime [clock clicks -milliseconds]
    set retVal [catch {image create photo -file $name} ph]
    if { $retVal != 0 } {
	P "\n\tWarning: Cannot detect image file format. Trying again with -format."
	P "\tError message: $ph"
	set retVal [catch {image create photo -file $name -format $fmt} ph]
	if { $retVal != 0 } {
	    P "\tERROR: Cannot read image file with format option $fmt" 
	    P "\tError message: $ph"
	    return {}
	}
    }
    set eTime [clock clicks -milliseconds]
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $ph
}

# Load image data directly from a file into a photo image.
# Uses commands: set ph [image create photo] ; $ph read "fileName" 
# args maybe "-from ..." and/or "-to ..." option.
proc readPhotoFile2 { name fmt width height args } {
    PN "File read 2: "

    set sTime [clock clicks -milliseconds]
    if { $width < 0 && $height < 0 } {
        set ph [image create photo]
    } else {
        set ph [image create photo -width $width -height $height]
    }
    set retVal [catch {eval {$ph read $name} $args} errMsg]
    if { $retVal != 0 } {
	P "\n\tWarning: Cannot detect image file format. Trying again with -format."
	P "\tError message: $errMsg"
	set retVal [catch {eval {$ph read $name -format $fmt} $args} errMsg]
	if { $retVal != 0 } {
	    P "\tERROR: Cannot read image file with format option $fmt" 
	    P "\tError message: $errMsg"
	    return {}
	}
    }
    set eTime [clock clicks -milliseconds]
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $ph
}

# Load binary image data from a variable into a photo image.
# Uses commands: image create photo -data $imgData
proc readPhotoBinary1 { name fmt args } {
    PN "Binary read 1: "

    set sTime [clock clicks -milliseconds]
    set retVal [catch {open $name r} fp]
    if { $retVal != 0 } {
	P "\n\tERROR: Cannot open image file $name for binary reading."
	return {}
    }
    fconfigure $fp -translation binary
    set imgData [read $fp [file size $name]]
    close $fp

    set retVal [catch {image create photo -data $imgData} ph]
    if { $retVal != 0 } {
	P "\n\tWarning: Cannot detect image file format. Trying again with -format."
	P "\tError message: $ph"
	set retVal [catch {image create photo -data $imgData -format $fmt} ph]
	if { $retVal != 0 } {
	    P "\tERROR: Cannot create photo from binary image data." 
	    P "\tError message: $ph"
	    return {}
	}
    }
    set eTime [clock clicks -milliseconds]
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $ph
}

# Load binary image data from a variable into a photo image.
# Uses commands: set ph [image create photo] ; $ph put $imgData
# args maybe "-to ..." option.
proc readPhotoBinary2 { name fmt width height args } {
    PN "Binary read 2: "

    set sTime [clock clicks -milliseconds]
    set retVal [catch {open $name r} fp]
    if { $retVal != 0 } {
	P "\n\tERROR: Cannot open image file $name for binary reading."
	return {}
    }
    fconfigure $fp -translation binary
    set imgData [read $fp [file size $name]]
    close $fp

    if { $width < 0 && $height < 0 } {
        set ph [image create photo]
    } else {
        set ph [image create photo -width $width -height $height]
    }
    set retVal [catch {eval {$ph put $imgData} $args} errMsg]
    if { $retVal != 0 } {
	P "\n\tWarning: Cannot detect image file format. Trying again with -format."
	P "\tError message: $errMsg"
	set retVal [catch {eval {$ph put $imgData -format $fmt} $args} errMsg]
	if { $retVal != 0 } {
	    P "\tERROR: Cannot create photo from binary image data."
	    P "\tError message: $errMsg"
	    return {}
	}
    }
    set eTime [clock clicks -milliseconds]
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $ph
}

# Load uuencoded image data from a variable into a photo image.
# Uses commands: set ph [image create photo] ; $ph put $imgData
proc readPhotoString { str fmt width height args } {
    PN "String read: "

    set sTime [clock clicks -milliseconds]
    if { $width < 0 && $height < 0 } {
        set ph [image create photo]
    } else {
        set ph [image create photo -width $width -height $height]
    }
    set retVal [catch {eval {$ph put $str} $args}]
    if { $retVal != 0 } {
	P "\n\tWarning: Cannot detect image string format. Trying again with -format."
	set retVal [catch {eval {$ph put $str -format $fmt} $args}]
	if { $retVal != 0 } {
	    P "\tERROR: Cannot read image string with format option: $fmt" 
	    return {}
	}
    }
    set eTime [clock clicks -milliseconds]
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $ph
}

proc writePhotoFile { ph name fmt del args } {
    PN "File write: "

    set sTime [clock clicks -milliseconds]
    set retVal [catch {eval {$ph write $name -format $fmt} $args}]
    set eTime [clock clicks -milliseconds]

    if { $retVal != 0 } {
	P "\n\tERROR: Cannot write image file $name (Format: $fmt)" 
	return
    }
    if { $del } {
	image delete $ph
    }
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
}

proc writePhotoString { ph fmt del args } {
    PN "String write: "

    set sTime [clock clicks -milliseconds]
    set retVal [catch {eval {$ph data -format $fmt} $args} str]
    set eTime [clock clicks -milliseconds]
    if { $retVal != 0 } {
	P "\n\tERROR: Cannot write image to string (Format: $fmt)" 
	P "\tError message: $str"
	return ""
    }
    if { $del } {
	image delete $ph
    }
    PN "[format "%.2f " [expr ($eTime - $sTime) / 1.0E3]]"
    return $str
}

proc createErrImg {} {
    set retVal [catch {image create photo -data [unsupportedImg]} errImg]
    if { $retVal != 0 } {
	P "FATAL ERROR: Cannot load uuencode GIF image into canvas."
	P "             Test will be cancelled."
	exit 1
    }
    return $errImg
}

proc getCanvasPhoto { canvId } {
    set retVal [catch {image create photo -format window -data $canvId} ph]
    if { $retVal != 0 } {
	P "\n\tFATAL ERROR: Cannot create photo from canvas window"
	exit 1
    }
    return $ph
}

proc delayedUpdate {} {
    update 
    after 200
}

proc drawInfo { x y color } {
    set size 10
    set tx [expr $x + $size * 2]
    .t.c create rectangle $x $y [expr $x + $size] [expr $y + $size] -fill $color
    .t.c create text $tx $y -anchor nw -fill $color -text "$color box"
    delayedUpdate
}

proc drawTestCanvas { imgVersion} {
    if { [catch {toplevel .t -visual truecolor}] } {
	toplevel .t
    }
    wm title .t "Canvas window"
    wm geometry .t "+0+30"

    canvas .t.c -bg gray -width 240 -height 220
    pack .t.c

    P "Loading uuencoded GIF image into canvas .." 
    set retVal [catch {image create photo -data [pwrdLogo]} phImg]
    if { $retVal != 0 } {
	P "FATAL ERROR: Cannot load uuencode GIF image into canvas." 
	P "             Test will be cancelled." 
	exit 1
    }

    .t.c create image 0 0 -anchor nw -tags MyImage
    .t.c itemconfigure MyImage -image $phImg

    P "Drawing text and rectangles into canvas .." 
    .t.c create rectangle 1 1 239 219 -outline black
    .t.c create rectangle 3 3 237 217 -outline green -width 2
    delayedUpdate

    drawInfo 140  10 black
    drawInfo 140  30 white
    drawInfo 140  50 red
    drawInfo 140  70 green
    drawInfo 140  90 blue
    drawInfo 140 110 cyan
    drawInfo 140 130 magenta
    drawInfo 140 150 yellow

    .t.c create text 140 170 -anchor nw -fill black -text "Created with:"
    delayedUpdate ; delayedUpdate
    .t.c create text 140 185 -anchor nw -fill black -text "Tcl [info patchlevel]"
    .t.c create text 140 200 -anchor nw -fill black -text "tkImg $imgVersion"
    update
}
