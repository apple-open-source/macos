#!/bin/sh
# The next line restarts using wish8.3 \
exec wish8.3 $0 ${1+"$@"}

source [file join [file dirname [info script]] imgmsg.tcl]

proc mmc string {
    regsub & $string {} string
    return $string
}

proc mml string {
    return [string first & $string]
}

#
# Make the Image format available.
#

package require Img

#
# Initialization of some global variables
#

set tkvPriv(count) 0
set tkvPriv(currentdir) [pwd]
set tkvPriv(defaultext) .gif

set tkvPriv(types) [list \
    [list [mc "Image Files"]	{.bmp}		] \
    [list [mc "Image Files"]	{.gif}		] \
    [list [mc "Image Files"]	{.jpeg .jpg}	] \
    [list [mc "Image Files"]	{.png}		] \
    [list [mc "Image Files"]	{.tiff .tif}	] \
    [list [mc "Image Files"]	{.xbm}		] \
    [list [mc "Image Files"]	{.xpm}		] \
    [list [mc "Image Files"]	{.ps .eps}		] \
    [list "BMP [mc Files]"		{.bmp}		] \
    [list "GIF [mc Files]"		{.gif}		] \
    [list "JPEG [mc Files]"		{.jpeg .jpg}	] \
    [list "PNG [mc Files]"		{.png}		] \
    [list "TIFF [mc Files]"		{.tiff .tif}	] \
    [list "XBM [mc Files]"		{.xbm}		] \
    [list "XPM [mc Files]"		{.xpm}		] \
    [list "Postscript [mc Files]"	{.ps .eps}		] \
    [list "GIF [mc Files]"		{}			GIFF] \
    [list "JPEG [mc Files]"		{}			JPEG] \
    [list "PNG [mc Files]"		{}			PNGF] \
    [list "TIFF [mc Files]"		{}			TIFF] \
    [list [mc {All files}]		*] \
]

proc Menu {base name} {
    set menu [ConCat $base menu]
    if {![winfo exists $menu]} {
	menu $menu
	$base configure -menu $menu
    }
    set accelerator [string toupper [string index $name 0]]
    set text [mc "&$accelerator[string range $name 1 end]"]
    set name $menu.$name
    if {![winfo exists $name]} {
	menu $name
	$menu add cascade -label [mmc $text] -menu $name -underline [mml $text]
    }
    return $name
}

#
# Small proc to concatenate window pathnames
#
proc ConCat args {
    regsub -all {[ 	\.]+} $args . args
    return $args
}

#
#  Create a new image window
#

proc image_window {{window {}}} {
    global tkvPriv
    if {![string compare $window {}]} {
	set window .image$tkvPriv(count)
	incr tkvPriv(count)
    }
    if {[winfo exists $window]} {
	catch {eval destroy [winfo children $window]}
    } else {
	toplevel $window
    }
    wm title $window [mc {Viewer}]
    set frame [ConCat $window frame]
    label $frame -relief sunken -bg white -bd 2 -anchor nw
    set w [Menu $window file]
    $w configure -tearoff 0
    set text [mc &Open]
    $w add command -label [mmc $text] -command [list load_image $window] -underline [mml $text]
    set text [mc &Save]
    $w add command -label [mmc $text] -command [list save_image $window] -underline [mml $text]
    $w add separator
    set text [mc &Close]
    $w add command -label [mmc $text] -command [list destroy $window] -underline [mml $text]
    set text [mc E&xit]
    $w add command -label [mmc $text] -command [list destroy .] -underline [mml $text]

    set w [Menu $window images]
    set w [Menu $window help]
    $w configure -tearoff 0
    set text [mc &About]
    $w add command -label [mmc $text] -command About -underline [mml $text]

    catch {wm geometry $window 200x200}
    pack $frame -side top -expand y -fill both
    return $frame
}

proc register_image {w name} {
    set menu [Menu $w images]
    set item [file tail [lindex $name 0]]
    if {[llength $name] > 1} {
	append item " [lindex $name 1]"
    }
    $menu add command -label $item -command \
	[list show_image $w $name]
}

proc show_image {window name} {
    [ConCat $window frame] configure -image $name
    catch {wm geometry $window {}}
}

proc load_image window {
    global tkvPriv
    set filename [tk_getOpenFile -filetypes $tkvPriv(types) -parent \
	    $window -initialdir $tkvPriv(currentdir)]
    if {[string compare $filename {}]} {
	set imagename [list $filename]
	image create photo $imagename -file $filename
	register_image $window $imagename
	show_image $window $imagename
	set tkvPriv(currentdir) [file dirname $filename]
    }
}

proc save_image window {
    global tkvPriv
    set img [[ConCat $window frame] cget -image]
    set filename [tk_getSaveFile -filetypes $tkvPriv(types) -parent \
	    $window -initialdir $tkvPriv(currentdir) -defaultextension .gif \
	    -initialfile [file rootname [lindex $img 0]].gif]
    if {[string compare $filename {}]} {
	$img write $filename -format gif
	set tkvPriv(currentdir) [file dirname $filename]
    }
}

proc About {} {
    tk_dialog .about "[mmc [mc &About]] tkv.tcl" "[mc {Tiny Image viewer}]\n[mc {written by}]\
	    Jan Nijtmans <j.nijtmans@chello.nl>" {} 0 [mc O.K.]
}

set w [lindex $argv 1]
if {![string compare $w {}]} {
    set w .
}
set filename [lindex $argv 0]

image_window $w
if {[string compare $filename {}]} {
    set imagename [list $filename]
    image create photo $imagename -file $filename
    register_image $w $imagename
    show_image $w $imagename
}
