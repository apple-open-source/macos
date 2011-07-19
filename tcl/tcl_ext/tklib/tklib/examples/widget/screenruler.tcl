#!/usr/bin/env tclsh

package require Tk 8.4
set noimg [catch {package require img::png}] ; # for saving images

# We are the main script being run - show ourselves
wm withdraw .

package require widget::screenruler
set dlg [widget::screenruler .r -title "Screen Ruler" -width 250 -height 100]

if {[tk windowingsystem] eq "aqua"} {
    set CTRL    "Command-"
    set CONTROL Command
} else {
    set CTRL    Ctrl+
    set CONTROL Control
}

upvar \#0 [$dlg info vars reflect] reflect
if {[lsearch -exact [image names] $reflect(image)] != -1} {
    # We have a reflectable desktop

    $dlg menu add separator
    $dlg menu add command -label "Save Image" -accelerator ${CTRL}s \
	-underline 0 -command [list save $dlg]
    bind $dlg <$CONTROL-s> [list save $dlg]
}

if {[tk windowingsystem] eq "x11"} {
    # Add hack to control overrideredirect as some wms (eg KDE) have focus
    # issues on the overrideredirect window
    set override [expr {![wm overrideredirect $dlg]}]
    $dlg menu add separator
    $dlg menu add checkbutton -label "Window Decoration" -variable ::override \
	-command [list override $dlg]
    proc override {w} {
	wm withdraw $w
	wm overrideredirect $w [expr {! $::override}]
	wm deiconify $w
    }
    wm protocol $dlg WM_DELETE_WINDOW exit
}

$dlg menu add separator
$dlg menu add command -label "Exit" \
    -underline 1 -accelerator ${CTRL}q -command { exit }
bind $dlg <$CONTROL-q> { exit }

package require comm
$dlg menu add separator
$dlg menu add command -label "COMM: [comm::comm self]" -state disabled

focus -force $dlg
$dlg display
$dlg configure -alpha 0.8

if {$::argc} {
    eval [linsert $argv 0 $dlg configure]
}

set LASTDIR  [pwd]

proc save {dlg} {
    global LASTDIR
    variable [$dlg info vars reflect]
    after cancel $reflect(id)

    if {$::noimg} {
	set filetypes {
	    {"All Image Files" {.gif .ppm}}
	}
	set re {\.(gif|ppm)$}
    } else {
	set filetypes {
	    {"All Image Files" {.gif .png .ppm}}
	    {"PNG Images" .png}
	}
	set re {\.(gif|ppm|png)$}
    }
    lappend filetypes {"GIF Images" .gif} {"PPM Images" .ppm} {"All Files" *}
    set file [tk_getSaveFile -parent $dlg -title "Save Image to File" \
		  -initialdir $LASTDIR -filetypes $filetypes]

    if {$file ne ""} {
	set LASTDIR [file dirname $file]
	if {![regexp -nocase $re $file -> ext]} {
	    tk_messageBox -title "Unrecognized Extension" \
		-parent $dlg -icon error -type ok \
		-message "Unknown file type to save for file\
		\"[file tail $file]\"\nPlease use .gif, .ppm or .png."
	} elseif {[catch {$reflect(image) write $file \
			      -format [string tolower $ext]} err]} {
	    tk_messageBox -title "Error Writing File" \
		-parent $dlg -icon error -type ok \
		-message "Error writing to file \"$file\":\n$err"
	}
    }

    $dlg configure -reflect [$dlg cget -reflect]
}

