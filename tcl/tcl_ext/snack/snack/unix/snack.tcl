# 
# Copyright (C) 1997-99 Kare Sjolander <kare@speech.kth.se>
#
# This file is part of the Snack sound extension for Tcl/Tk.
# The latest version can be found at http://www.speech.kth.se/snack/
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

package provide snack 2.2

# Set playback latency according to the environment variable PLAYLATENCY

if {$::tcl_platform(platform) == "unix"} {
    if {[info exists env(PLAYLATENCY)] && $env(PLAYLATENCY) > 0} {
	snack::audio playLatency $env(PLAYLATENCY)
    }
}

namespace eval snack {
    namespace export gainBox get* add* menu* frequencyAxis timeAxis \
	    createIcons mixerDialog sound audio mixer debug

    #
    # Gain control dialog
    #

    proc gainBox flags {
	variable gainbox
	
	catch {destroy .snackGainBox}
	toplevel .snackGainBox
	wm title .snackGainBox {Gain Control Panel}
	
	if {[string match *p* $flags]} {
	    set gainbox(play) [snack::audio play_gain]
	    pack [scale .snackGainBox.s -label {Play volume} -orient horiz \
		    -variable snack::gainbox(play) \
		    -command {snack::audio play_gain} \
		    -length 200]
	}

	if {[snack::mixer inputs] != ""} {
	    if {[string match *r* $flags]} {
		set gainbox(rec)  [snack::audio record_gain]
		pack [scale .snackGainBox.s2 -label {Record gain} \
			-orient horiz \
			-variable snack::gainbox(rec) \
			-command {snack::audio record_gain} \
			-length 200]
	    }
	}
	pack [button .snackGainBox.exitB -text Close -command {destroy .snackGainBox}]
    }

    #
    # Snack mixer dialog
    #

    proc flipScaleValue {scaleVar var args} {
     set $var [expr 100-[set $scaleVar]]
    }

    proc mixerDialog {} {
	set wi .snackMixerDialog
	catch {destroy $wi}
	toplevel $wi
	wm title $wi "Mixer"

#	pack [frame $wi.f0]
#	label $wi.f0.l -text "Mixer device:"
	
#	set outDevList [snack::mixer devices]
#	eval tk_optionMenu $wi.f0.om mixerDev $outDevList
#	pack $wi.f0.l $wi.f0.om -side left

	pack [frame $wi.f] -expand yes -fill both
	foreach line [snack::mixer lines] {
	    pack [frame $wi.f.g$line -bd 1 -relief solid] -side left \
		    -expand yes -fill both
	    pack [label $wi.f.g$line.l -text $line]
	    if {[snack::mixer channels $line] == "Mono"} {
		snack::mixer volume $line snack::v(r$line)
	    } else {
	      snack::mixer volume $line snack::v(l$line) snack::v(r$line)
	      if {[info exists tile::version]} {
	       pack [ttk::scale $wi.f.g$line.e -from 0 -to 100 -show no -orient vertical \
			 -var snack::v(lI$line) -command  [namespace code [list flipScaleValue ::snack::v(lI$line) ::snack::v(l$line)]]] -side left -expand yes -fill y
	       set snack::v(lI$line) [expr 100-[lindex [snack::mixer volume $line] end]]
	       $wi.f.g$line.e set $snack::v(lI$line)
	      } else {
	       pack [scale $wi.f.g$line.e -from 100 -to 0 -show no -orient vertical \
			 -var snack::v(l$line)] -side left -expand yes -fill both
	      }
	    }
  	    if {[info exists tile::version]} {
	     pack [ttk::scale $wi.f.g$line.s -from 0 -to 100 -show no -orient vertical \
		       -var snack::v(rI$line) -command  [namespace code [list flipScaleValue ::snack::v(rI$line) ::snack::v(r$line)]]] -expand yes -fill y
	     set snack::v(rI$line) [expr 100-[lindex [snack::mixer volume $line] end]]
	     $wi.f.g$line.s set $snack::v(rI$line)
	    } else {
	     pack [scale $wi.f.g$line.s -from 100 -to 0 -show no -orient vertical \
		       -var snack::v(r$line)] -expand yes -fill both
	    }
	}
	
	pack [frame $wi.f.f2] -side left
	
	if {[snack::mixer inputs] != ""} {
	    pack [label $wi.f.f2.li -text "Input jacks:"]
	    foreach jack [snack::mixer inputs] {
		snack::mixer input $jack [namespace current]::v(in$jack)
		pack [checkbutton $wi.f.f2.b$jack -text $jack \
			-variable [namespace current]::v(in$jack)] \
			-anchor w
	    }
	}
	if {[snack::mixer outputs] != ""} {
	    pack [label $wi.f.f2.lo -text "Output jacks:"]
	    foreach jack [snack::mixer outputs] {
		snack::mixer output $jack [namespace current]::v(out$jack)
		pack [checkbutton $wi.f.f2.b$jack -text $jack \
			-variable [namespace current]::v(out$jack)] \
			-anchor w
	    }
	}
	pack [button $wi.b1 -text Close -command "destroy $wi"]
    }

    #
    # Snack filename dialog
    #

    proc getOpenFile {args} {
	upvar #0 __snack_args data
	
	set specs {
	    {-title       "" "" "Open file"}
	    {-initialdir  "" "" "."}
	    {-initialfile "" "" ""}
	    {-multiple    "" "" 0}
	    {-format      "" "" "none"}
	}
	
	tclParseConfigSpec __snack_args $specs "" $args
	
	if {$data(-format) == "none"} {
	    if {$data(-initialfile) != ""} {
		set data(-format) [ext2fmt [file extension $data(-initialfile)]]
	    } else {
		set data(-format) WAV
	    }
	}
	if {$data(-format) == ""} {
	    set data(-format) RAW
	}
	set data(-format) [string toupper $data(-format)]
	if {$data(-initialdir) == ""} {
	    set data(-initialdir) "."
	}
        if {[string match Darwin $::tcl_platform(os)]} {
	 return [tk_getOpenFile -title $data(-title) \
		    -multiple $data(-multiple) \
		    -filetypes [loadTypes $data(-format)] \
		    -defaultextension [fmt2ext $data(-format)] \
		     -initialdir $data(-initialdir)]
	}
	# Later Tcl's allow multiple files returned as a list
	if {$::tcl_version <= 8.3} {
	    set res [tk_getOpenFile -title $data(-title) \
		    -filetypes [loadTypes $data(-format)] \
		    -defaultextension [fmt2ext $data(-format)] \
		    -initialdir $data(-initialdir) \
		    -initialfile $data(-initialfile)]
	} else {
	    set res [tk_getOpenFile -title $data(-title) \
		    -multiple $data(-multiple) \
		    -filetypes [loadTypes $data(-format)] \
		    -defaultextension [fmt2ext $data(-format)] \
		    -initialdir $data(-initialdir) \
		    -initialfile $data(-initialfile)]
	}
	return $res
    }

    set loadTypes ""

    proc addLoadTypes {typelist fmtlist} {
	variable loadTypes
	variable filebox
	
	set loadTypes $typelist
	set i 9 ; # Needs updating when adding new formats
	foreach fmt $fmtlist { 
	    set filebox(l$fmt) $i
	    incr i
	}
    }

    proc loadTypes fmt {
	variable loadTypes
	variable filebox

	if {$::tcl_platform(platform) == "windows"} {
	    set l [concat {{{MS Wav Files} {.wav}} {{Smp Files} {.smp}} {{Snd Files} {.snd}} {{AU Files} {.au}} {{AIFF Files} {.aif}} {{AIFF Files} {.aiff}} {{Waves Files} {.sd}} {{MP3 Files} {.mp3}} {{CSL Files} {.nsp}}} $loadTypes {{{All Files} * }}]
	} else {
	    set l [concat {{{MS Wav Files} {.wav .WAV}} {{Smp Files} {.smp .SMP}} {{Snd Files} {.snd .SND}} {{AU Files} {.au .AU}} {{AIFF Files} {.aif .AIF}} {{AIFF Files} {.aiff .AIFF}} {{Waves Files} {.sd .SD}} {{MP3 Files} {.mp3 .MP3}} {{CSL Files} {.nsp .NSP}}} $loadTypes {{{All Files} * }}]
	}
	return [swapListElem $l $filebox(l$fmt)]
    }

    variable filebox
    set filebox(RAW) .raw
    set filebox(SMP) .smp
    set filebox(AU) .au
    set filebox(WAV) .wav
    set filebox(SD) .sd
    set filebox(SND) .snd
    set filebox(AIFF) .aif
    set filebox(MP3) .mp3
    set filebox(CSL) .nsp

    set filebox(lWAV) 0
    set filebox(lSMP) 1
    set filebox(lSND) 2
    set filebox(lAU)  3
    set filebox(lAIFF)  4
    # skip 2 because of aif and aiff
    set filebox(lSD)  6
    set filebox(lMP3)  7
    set filebox(lCSL)  8
    set filebox(lRAW) end
    # Do not forget to update indexes
    set filebox(sWAV) 0
    set filebox(sSMP) 1
    set filebox(sSND) 2
    set filebox(sAU)  3
    set filebox(sAIFF)  4
    # skip 2 because of aif and aiff
    set filebox(sCSL)  6
    set filebox(sRAW) end

    proc fmt2ext fmt {
	variable filebox

	return $filebox($fmt)
    }

    proc addExtTypes extlist {
	variable filebox

	foreach pair $extlist {
	    set filebox([lindex $pair 0]) [lindex $pair 1]
	}
    }

    proc getSaveFile args {
	upvar #0 __snack_args data

	set specs {
	    {-title       "" "" "Save file"}
	    {-initialdir  "" "" "."}
	    {-initialfile "" "" ""}
	    {-format      "" "" "none"}
	}

	tclParseConfigSpec __snack_args $specs "" $args

	if {$data(-format) == "none"} {
	    if {$data(-initialfile) != ""} {
		set data(-format) [ext2fmt [file extension $data(-initialfile)]]
	    } else {
		set data(-format) WAV
	    }
	}
	if {$data(-format) == ""} {
	    set data(-format) RAW
	}
	set data(-format) [string toupper $data(-format)]
	if {$data(-initialdir) == ""} {
	    set data(-initialdir) "."
	}
	if {[string match macintosh $::tcl_platform(platform)]} {
	  set tmp [tk_getSaveFile -title $data(-title) \
	      -initialdir $data(-initialdir) -initialfile $data(-initialfile)]
	  if {[string compare [file ext $tmp] ""] == 0} {
	    append tmp [fmt2ext $data(-format)]
	  }
	  return $tmp
	} else {
	  return [tk_getSaveFile -title $data(-title) \
	      -filetypes [saveTypes $data(-format)] \
	      -defaultextension [fmt2ext $data(-format)] \
	      -initialdir $data(-initialdir) -initialfile $data(-initialfile)]
	}
    }

    set saveTypes ""

    proc addSaveTypes {typelist fmtlist} {
	variable saveTypes
	variable filebox

	set saveTypes $typelist
	set j 7 ; # Needs updating when adding new formats
	foreach fmt $fmtlist {
	    set filebox(s$fmt) $j
	    incr j
	}
    }

    proc saveTypes fmt {
	variable saveTypes 
	variable filebox
	
	if {[info exists filebox(s$fmt)] == 0} {
	    set fmt RAW
	}
	if {$::tcl_platform(platform) == "windows"} {
	    set l [concat {{{MS Wav Files} {.wav}} {{Smp Files} {.smp}} {{Snd Files} {.snd}} {{AU Files} {.au}} {{AIFF Files} {.aif}} {{AIFF Files} {.aiff}} {{CSL Files} {.nsp}}} $saveTypes {{{All Files} * }}]
	} else {
	    set l [concat {{{MS Wav Files} {.wav .WAV}} {{Smp Files} {.smp .SMP}} {{Snd Files} {.snd .SND}} {{AU Files} {.au .AU}} {{AIFF Files} {.aif .AIF}} {{AIFF Files} {.aiff .AIFF}} {{CSL Files} {.nsp .NSP}}} $saveTypes {{{All Files} * }}]
	}
	return [swapListElem $l $filebox(s$fmt)]
    }

    proc swapListElem {l n} {
	set tmp [lindex $l $n]
	set l [lreplace $l $n $n]
	return [linsert $l 0 $tmp]
    }

    set filebox(.wav) WAV
    set filebox(.smp) SMP
    set filebox(.au) AU
    set filebox(.raw) RAW
    set filebox(.snd) SND
    set filebox(.sd) SD
    set filebox(.aif) AIFF
    set filebox(.aiff) AIFF
    set filebox(.mp3) MP3
    set filebox(.nsp) CSL
    set filebox() WAV

    proc ext2fmt ext {
	variable filebox

	return $filebox($ext)
    }

    #
    # Menus
    #

    proc menuInit { {m .menubar} } {
	variable menu

	menu $m
	[winfo parent $m] configure -menu $m
	set menu(menubar) $m
	set menu(uid) 0
    }

    proc menuPane {label {u 0} {postcommand ""}} {
	variable menu
	
	if [info exists menu(menu,$label)] {
	    error "Menu $label already defined"
	}
	if {$label == "Help"} {
	    set name $menu(menubar).help
	} else {
	    set name $menu(menubar).mb$menu(uid)
	}
	set m [menu $name -tearoff 1 -postcommand $postcommand]
	$menu(menubar) add cascade -label $label -menu $name -underline $u
	incr menu(uid)
	set menu(menu,$label) $m
	return $m
    }

    proc menuDelete {menuName label} {
	variable menu

	set m [menuGet $menuName]
	if [catch {$m index $label} index] {
	    error "$label not in menu $menuName"
	}
	[menuGet $menuName] delete $index
    }

    proc menuDeleteByIndex {menuName index} {
	[menuGet $menuName] delete $index
    }

    proc menuGet menuName {
	variable menu
	if [catch {set menu(menu,$menuName)} m] {
	    return -code error "No such menu: $menuName"
	}
	return $m
    }

    proc menuCommand {menuName label command} {
	[menuGet $menuName] add command -label $label -command $command
    }

    proc menuCheck {menuName label var {command {}} } {
	variable menu

	[menuGet $menuName] add check -label $label -command $command \
		-variable $var
    }

    proc menuRadio {menuName label var {val {}} {command {}} } {
	variable menu
	
	if {[string length $val] == 0} {
	    set val $label
	}
	[menuGet $menuName] add radio -label $label -command $command \
		-value $val -variable $var
    }

    proc menuSeparator menuName {
	variable menu

	[menuGet $menuName] add separator
    }

    proc menuCascade {menuName label} {
	variable menu

	set m [menuGet $menuName]
	if [info exists menu(menu,$label)] {
	    error "Menu $label already defined"
	}
	set sub $m.sub$menu(uid)
	incr menu(uid)
	menu $sub -tearoff 0
	$m add cascade -label $label -menu $sub
	set menu(menu,$label) $sub
	return $sub
    }

    proc menuBind {what char menuName label} {
	variable menu

	set m [menuGet $menuName]
	if [catch {$m index $label} index] {
	    error "$label not in menu $menuName"
	}
	set command [$m entrycget $index -command]
	if {$::tcl_platform(platform) == "unix"} {
	    bind $what <Alt-$char> $command
	    $m entryconfigure $index -accelerator Alt-$char
	} else {
	    bind $what <Control-$char> $command
	    set char [string toupper $char]
	    $m entryconfigure $index -accelerator Ctrl-$char
	}
    }

    proc menuEntryOff {menuName label} {
	variable menu
	
	set m [menuGet $menuName]
	if [catch {$m index $label} index] {
	    error "$label not in menu $menuName"
	}
	$m entryconfigure $index -state disabled
    }

    proc menuEntryOn {menuName label} {
	variable menu
	
	set m [menuGet $menuName]
	if [catch {$m index $label} index] {
	    error "$label not in menu $menuName"
	}
	$m entryconfigure $index -state normal
    }

    #
    # Vertical frequency axis
    #

    proc frequencyAxis {canvas x y width height args} {
	array set a [list \
		-tags snack_y_axis \
		-font {Helvetica 8} \
		-topfr 8000 \
		-fill black \
		-draw0 0
	]
        if {[string match unix $::tcl_platform(platform)] } {
	 set a(-font) {Helvetica 10}
	}
	array set a $args

	if {$height <= 0} return
	set ticklist [list 10 20 50 100 200 500 1000 2000 5000 \
		10000 20000 50000 100000 200000 500000 1000000]
	set npt 10
	set dy [expr {double($height * $npt) / $a(-topfr)}]

	while {$dy < [font metrics $a(-font) -linespace]} {
	    foreach elem $ticklist {
		if {$elem <= $npt} {
		    continue
		}
		set npt $elem
		break
	    }
	    set dy [expr {double($height * $npt) / $a(-topfr)}]
	}

	if {$npt < 1000} { 
	    set hztext Hz
	} else {
	    set hztext kHz
	}

	if $a(-draw0) {
	    set i0 0
	    set j0 0
	} else {
	    set i0 $dy
	    set j0 1
	}

	for {set i $i0; set j $j0} {$i < $height} {set i [expr {$i+$dy}]; incr j} {
	    set yc [expr {$height + $y - $i}]

	    if {$npt < 1000} { 
		set t [expr {$j * $npt}]
	    } else {
		set t [expr {$j * $npt / 1000}]
	    }
	    if {$yc > [expr {8 + $y}]} {
		if {[expr {$yc - [font metrics $a(-font) -ascent]}] > \
			[expr {$y + [font metrics $a(-font) -linespace]}] ||
		[font measure $a(-font) $hztext]  < \
			[expr {$width - 8 - [font measure $a(-font) $t]}]} {
		    $canvas create text [expr {$x +$width - 8}] [expr {$yc-2}]\
			    -text $t -fill $a(-fill)\
			    -font $a(-font) -anchor e -tags $a(-tags)
		}
		$canvas create line [expr {$x + $width - 5}] $yc \
			[expr {$x + $width}]\
			$yc -tags $a(-tags) -fill $a(-fill)
	    }
	}
	$canvas create text [expr {$x + 2}] [expr {$y + 1}] -text $hztext \
		-font $a(-font) -anchor nw -tags $a(-tags) -fill $a(-fill)

	return $npt
    }

    #
    # Horizontal time axis
    #

    proc timeAxis {canvas ox oy width height pps args} {
	array set a [list \
		-tags snack_t_axis \
		-font {Helvetica 8} \
		-starttime 0.0 \
		-fill black \
		-format time \
		-draw0 0 \
		-drawvisible 0
	]
        if {[string match unix $::tcl_platform(platform)] } {
	 set a(-font) {Helvetica 10}
	}
	array set a $args

	if {$pps <= 0.004} return

        switch -- $a(-format) {
	 time -
	 seconds {
	  set deltalist [list .0001 .0002 .0005 .001 .002 .005 \
			     .01 .02 .05 .1 .2 .5 1 2 5 \
			     10 20 30 60 120 240 360 600 900 1800 3600 7200 14400]
	 }
	 "PAL frames" {
	  set deltalist [list .04 .08 .4 .8 2 4 \
			     10 20 50 100 200 500 1000 2000 5000 10000 20000]
	 }
	 "NTSC frames" {
	  set deltalist [list .03333333333334 .0666666666667 \
			     .3333333333334 .666666666667 1 2 4 \
			     10 20 50 100 200 500 1000 2000 5000 10000 20000]
	 }
	 "10ms frames" {
	  set deltalist [list .01 .02 .05 .1 .2 .5 1 2 5 \
			     10 20 50 100 200 500 1000 2000 5000 10000 20000]
	 }
	}

	set majTickH [expr {$height - [font metrics $a(-font) -linespace]}]
	set minTickH [expr {$majTickH / 2}]

# Create a typical time label

	set maxtime [expr {double($width) / $pps + $a(-starttime)}]
	if {$maxtime < 60} {
	    set wtime 00
	} elseif {$maxtime < 3600} {
	    set wtime 00:00
	} else {
	    set wtime 00:00:00
	}
	if {$pps > 50} {
	    append wtime .0
	} elseif {$pps > 500} {
	    append wtime .00
	} elseif {$pps > 5000} {
	    append wtime .000
	} elseif {$pps > 50000} {
	    append wtime .0000
	}

# Compute the distance in pixels (and time) between tick marks

	set dx [expr {10+[font measure $a(-font) $wtime]}]
        set dt [expr {double($dx) / $pps}]

	foreach elem $deltalist {
	    if {$elem <= $dt} {
		continue
	    }
	    set dt $elem
	    break
	}
	set dx [expr {$pps * $dt}]

	if {$dt < 0.00099} {
	    set ndec 4
	} elseif {$dt < 0.0099} {
	    set ndec 3
	} elseif {$dt < 0.099} {
	    set ndec 2
	} else {
	    set ndec 1
	}
	
	if {$a(-starttime) > 0.0} {
	    set ft [expr {(int($a(-starttime) / $dt) + 1) * $dt}]
	    set fx [expr {$pps * ($ft - $a(-starttime))}]
	} else {
	    set ft 0
	    set fx 0.0
	}
	
	set lx [expr {($ox + $width) * [lindex [$canvas xview] 0] - 50}]
	set rx [expr {($ox + $width) * [lindex [$canvas xview] 1] + 50}]

	set jinit 0

	if {$a(-drawvisible)} {
         set jinit [expr {int($lx/$dx)}]
         set fx [expr {$fx + $jinit * $dx}]
	}

	for {set x $fx;set j $jinit} {$x < $width} \
		{set x [expr {$x+$dx}];incr j} {

	    if {$a(-drawvisible) && $x < $lx} continue
	    if {$a(-drawvisible) && $x > $rx} break

	    switch -- $a(-format) {
	     time {
	      set t [expr {$j * $dt + $ft}]
	      
	      if {$maxtime < 60} {
	       set tmp [expr {int($t)}]
	      } elseif {$maxtime < 3600} {
	       set tmp x[clock format [expr {int($t)}] -format "%M:%S" -gmt 1]
	       regsub x0 $tmp "" tmp
	       regsub x $tmp "" tmp
	      } else {
	       set tmp [clock format [expr {int($t)}] -format "%H:%M:%S" -gmt 1]
	      }	    
	      if {$dt < 1.0} {
	       set t $tmp[string trimleft [format "%.${ndec}f" \
					       [expr {($t-int($t))}]] 0]
	      } else {
	       set t $tmp
	      }
	     }
	     "PAL frames" {
	      set t [expr {int($j * $dt * 25.0 + $ft)}]
	     }
	     "NTSC frames" {
	      set t [expr {int($j * $dt * 30.0 + $ft)}]
	     }
	     "10ms frames" {
	      set t [expr {int($j * $dt * 100.0 + $ft)}]
	     }
	     seconds {
	      set t [expr {double($j * $dt * 1.0 + $ft)}]
	     }
	    }
	    if {$a(-draw0) == 1 || $j > 0 || $a(-starttime) > 0.0} {
		$canvas create text [expr {$ox+$x}] [expr {$oy+$height}] \
			-text $t -font $a(-font) -anchor s -tags $a(-tags) \
			-fill $a(-fill)
	    }
	    $canvas create line [expr {$ox+$x}] $oy [expr {$ox+$x}] \
		    [expr {$oy+$majTickH}] -tags $a(-tags) -fill $a(-fill)

	    if {[string match *5 $dt] || [string match 5* $dt]} {
		set nt 5
	    } else {
		set nt 2
	    }
	    for {set k 1} {$k < $nt} {incr k} {
		set xc [expr {$k * $dx / $nt}]
		$canvas create line [expr {$ox+$x+$xc}] $oy \
			[expr {$ox+$x+$xc}] [expr {$oy+$minTickH}]\
			-tags $a(-tags) -fill $a(-fill)
	    }
	    
	}
    }

    #
    # Snack icons
    #

    variable icon

    set icon(new) R0lGODlhEAAQALMAAAAAAMbGxv///////////////////////////////////////////////////////yH5BAEAAAEALAAAAAAQABAAAAQwMMhJ6wQ4YyuB+OBmeeDnAWNpZhWpmu0bxrKAUu57X7VNy7tOLxjIqYiapIjDbDYjADs=

    set icon(open) R0lGODlhEAAQALMAAAAAAISEAMbGxv//AP///////////////////////////////////////////////yH5BAEAAAIALAAAAAAQABAAAAQ4UMhJq6Ug3wpm7xsHZqBFCsBADGTLrbCqllIaxzSKt3wmA4GgUPhZAYfDEQuZ9ByZAVqPF6paLxEAOw==

    set icon(save) R0lGODlhEAAQALMAAAAAAISEAMbGxv///////////////////////////////////////////////////yH5BAEAAAIALAAAAAAQABAAAAQ3UMhJqwQ4a30DsJfwiR4oYt1oASWpVuwYm7NLt6y3YQHe/8CfrLfL+HQcGwmZSXWYKOWpmDSBIgA7

    set icon(print) R0lGODlhEAAQALMAAAAAAISEhMbGxv//AP///////////////////////////////////////////////yH5BAEAAAIALAAAAAAQABAAAAQ5UMhJqwU450u67wCnAURYkZ9nUuRYbhKalkJoj1pdYxar40ATrxIoxn6WgTLGC4500J6N5Vz1roIIADs=

#    set icon(open) R0lGODlhFAATAOMAAAAAAFeEAKj/AYQAV5o2AP8BqP9bAQBXhC8AhJmZmWZmZszMzAGo/1sB/////9zc3CH5BAEAAAsALAAAAAAUABMAQARFcMlJq13ANc03uGAoTp+kACWpAUjruum4nAqI3hdOZVtz/zoS6/WKyY7I4wlnPKIqgB7waet1VqHoiliE+riw3PSXlEUAADs=

#    set icon(save) R0lGODlhFAATAOMAAAAAAAAAhAAA/wCEAACZmQD/AAD//4QAAISEAJmZmWZmZszMzP8AAP//AP///9zc3CH5BAEAAAsALAAAAAAUABMAQARBcMlJq5VACGDzvkAojiGocZWHUiopflcsL2p32lqu3+lJYrCZcCh0GVeTWi+Y5LGczY0RCtxZkVUXEEvzjbbEWQQAOw==

#    set icon(print) R0lGODlhFAATAOMAAAAAAAAAhAAA/wCEAACZmQD/AAD//4QAAISEAJmZmWZmZszMzP8AAP//AP///9zc3CH5BAEAAAsALAAAAAAUABMAQARHcMlJq53A6b2BEIAFjGQZXlTGdZX3vTAInmiNqqtGY3Ev76bgCGQrGo8toS3DdIycNWZTupMITbPUtfQBznyz6sLl84iRlAgAOw==

#    set icon(cut) R0lGODlhFAATAOMAAAAAAAAAhAAA/wCEAACZmQD/AAD//4QAAISEAJmZmWZmZszMzP8AAP//AP///9zc3CH5BAEAAAsALAAAAAAUABMAQAQ3cMlJq71LAYUvANPXVVsGjpImfiW6nK87aS8nS+x9gvvt/xgYzLUaEkVAI0r1ao1WMWSn1wNeIgA7

#    set icon(copy) R0lGODlhFAATAOMAAAAAAAAAhAAA/wCEAACZmQD/AAD//4QAAISEAJmZmWZmZszMzP8AAP//AP///9zc3CH5BAEAAAsALAAAAAAUABMAQARFcMlJq5XAZSB0FqBwjSTmnF45ASzbbZojqrTJyqgMjDAXwzNSaAiqGY+UVsuYQRGDluap49RcpLjcNJqjaqEXbxdJLkUAADs=

#    set icon(paste) R0lGODlhFAATAOMAAAAAAFeEAKj/AYQAV5o2AP8BqP9bAQBXhC8AhJmZmWZmZszMzAGo/1sB/////9zc3CH5BAEAAAsALAAAAAAUABMAQARTcMlJq11A6c01uFXjAGNJNpMCrKvEroqVcSJ5NjgK7tWsUr5PryNyGB04GdHE1PGe0OjrGcR8qkPPCwsk5nLCLu1oFCUnPk2RfHSqXms2cvetJyMAOw==

#    set icon(undo) R0lGODlhFAATAOMAAAAAAAAAhAAA/wCEAACZmQD/AAD//4QAAISEAJmZmWZmZszMzP8AAP//AP///9zc3CH5BAEAAAsALAAAAAAUABMAQAQ7cMlJq6UKALmpvmCIaWQJZqXidWJboWr1XSgpszTu7nyv1IBYyCSBgWyWjHAUnE2cnBKyGDxNo72sKwIAOw==

 set icon(cut) R0lGODlhEAAQALMAAAAAAAAAhMbGxv///////////////////////////////////////////////////yH5BAEAAAIALAAAAAAQABAAAAQvUMhJqwUTW6pF314GZhjwgeXImSrXTgEQvMIc3ONtS7PV77XNL0isDGs9YZKmigAAOw==

 set icon(copy) R0lGODlhEAAQALMAAAAAAAAAhMbGxv///////////////////////////////////////////////////yH5BAEAAAIALAAAAAAQABAAAAQ+UMhJqwA4WwqGH9gmdV8HiKYZrCz3ecG7TikWf3EwvkOM9a0a4MbTkXCgTMeoHPJgG5+yF31SLazsTMTtViIAOw==

 set icon(paste) R0lGODlhEAAQALMAAAAAAAAAhISEAISEhMbGxv//AP///////////////////////////////////////yH5BAEAAAQALAAAAAAQABAAAARMkMhJqwUYWJlxKZ3GCYMAgCdQDqLKXmUrGGE2vIRK7usu94GgMNDqDQKGZDI4AiqXhkDOiMxEhQCeAPlUEqm0UDTX4XbHlaFaumlHAAA7

 set icon(undo) R0lGODlhEAAQALMAAAAAhMbGxv///////////////////////////////////////////////////////yH5BAEAAAEALAAAAAAQABAAAAQgMMhJq704622BB93kUSAJlhUafJj6qaLJklxc33iuXxEAOw==

    set icon(redo) R0lGODlhFAATAKEAAMzMzGZmZgAAAAAAACH5BAEAAAAALAAAAAAUABMAAAI4hI+py+0fhBQhPDCztCzSkzWS4nFJZCLTMqrGxgrJBistmKUHqmo3jvBMdC9Z73MBEZPMpvOpKAAAOw==

    set icon(gain) R0lGODlhFAATAOMAAAAAAFpaWjMzZjMAmZlmmapV/729vY+Pj5mZ/+/v78zM/wAAAAAAAAAAAAAAAAAAACH5BAEAAAUALAAAAAAUABMAAARnsMhJqwU4a32T/6AHdF8WjhUAAoa6kqwhtyW8uUlG4Tl2DqoJjzUcIAIeyZAmAiBwyhUNADQCAsHCUoVBKBTERLQ0RRiftLGoPGgDk1qpC+N2qXPM5lscL/lAAj5CIYQ5gShaN4oVEQA7

    set icon(zoom) R0lGODlhFAATAMIAAAAAAF9fXwAA/8zM/8zMzP///wAAAAAAACH5BAEAAAQALAAAAAAUABMAAAM/SLrc/jBKGYAFYapaes0U0I0VIIkjaUZo2q1Q68IP5r5UcFtgbL8YTOhS+mgWFcFAeCQEBMre8WlpLqrWrCYBADs=

    set icon(zoomIn) R0lGODlhFAATAMIAAMzMzF9fXwAAAP///wAA/8zM/wAAAAAAACH5BAEAAAAALAAAAAAUABMAAANBCLrc/jBKGYQVYao6es2U0FlDJUjimFbocF1u+5JnhKldHAUB7mKom+oTupiImo2AUAAmAQECE/SMWp6LK3arSQAAOw==

    set icon(zoomOut) R0lGODlhFAATAMIAAMzMzF9fXwAAAP///wAA/8zM/wAAAAAAACH5BAEAAAAALAAAAAAUABMAAANCCLrc/jBKGYQVYao6es2U0I2VIIkjaUbidQ0r1LrtGaRj/AQ3boEyTA6DCV1KH82iQigUlYAAoQlUSi3QBTbL1SQAADs=

    set icon(play) R0lGODlhFQAVAKEAANnZ2QAAAP///////yH+FUNyZWF0ZWQgd2l0aCBUaGUgR0lNUAAh+QQBCgAAACwAAAAAFQAVAAACJISPqcvtD10IUc1Zz7157+h5Txg2pMicmESCqLt2VEbX9o1XBQA7

    set icon(pause) R0lGODlhFQAVAKEAANnZ2QAAAP///////yH+FUNyZWF0ZWQgd2l0aCBUaGUgR0lNUAAh+QQBCgAAACwAAAAAFQAVAAACLISPqcvtD12Y09DKbrC3aU55HfBlY7mUqKKO6emycGjSa9LSrx1H/g8MCiMFADs=
    set icon(stop) R0lGODlhFQAVAKEAANnZ2QAAAP///////yH+FUNyZWF0ZWQgd2l0aCBUaGUgR0lNUAAh+QQBCgAAACwAAAAAFQAVAAACJISPqcvtD12YtM5mc8C68n4xIPWBZXdqabZarSeOW0TX9o3bBQA7

    set icon(record) R0lGODlhFQAVAKEAANnZ2f8AAP///////yH+FUNyZWF0ZWQgd2l0aCBUaGUgR0lNUAAh+QQBCgAAACwAAAAAFQAVAAACJoSPqcvtDyMINMhZM8zcuq41ICeOVWl6S0p95pNu4BVe9o3n+lIAADs=

    proc createIcons {} {
	variable icon

	image create photo snackOpen  -data $icon(open)
	image create photo snackSave  -data $icon(save)
	image create photo snackPrint -data $icon(print)
	image create photo snackCut   -data $icon(cut)
	image create photo snackCopy  -data $icon(copy)
	image create photo snackPaste -data $icon(paste)
	image create photo snackUndo  -data $icon(undo)
	image create photo snackRedo  -data $icon(redo)
	image create photo snackGain  -data $icon(gain)
	image create photo snackZoom  -data $icon(zoom)
	image create photo snackZoomIn -data $icon(zoomIn)
	image create photo snackZoomOut -data $icon(zoomOut)
	image create photo snackPlay  -data $icon(play)
	image create photo snackPause  -data $icon(pause)
	image create photo snackStop  -data $icon(stop)
	image create photo snackRecord  -data $icon(record)
    }

    #
    # Support routines for shape files
    #

    proc deleteInvalidShapeFile {fileName} {
	if {$fileName == ""} return
	if ![file exists $fileName] return
	set shapeName ""
	if [file exists [file rootname $fileName].shape] {
	    set shapeName [file rootname $fileName].shape
	}
	if [file exists [file rootname [file tail $fileName]].shape] {
	    set shapeName [file rootname [file tail $fileName]].shape
	}
	if {$shapeName != ""} {
	    set fileTime [file mtime $fileName]
	    set shapeTime [file mtime $shapeName]
	    if {$fileTime > $shapeTime} {

		# Delete shape file if older than sound file

		file delete -force $shapeName
	    } else {
		set s [snack::sound]
		$s config -file $fileName
		set soundSize [expr {200 * [$s length -unit seconds] * \
		    [$s cget -channels]}]
		set shapeSize [file size $shapeName]
		if {[expr {$soundSize*0.95}] > $shapeSize || \
			[expr {$soundSize*1.05}] < $shapeSize} {

		    # Delete shape file with incorrect size

		    file delete -force $shapeName
		}
		$s destroy
	    }
	}
    }

    proc makeShapeFileDeleteable {fileName} {
	if {$::tcl_platform(platform) == "unix"} {
	    if [file exists [file rootname $fileName].shape] {
		set shapeName [file rootname $fileName].shape
		catch {file attributes $shapeName -permissions 0777}
	    }
	    if [file exists [file rootname [file tail $fileName]].shape] {
		set shapeName [file rootname [file tail $fileName]].shape
		catch {file attributes $shapeName -permissions 0777}
	    }
	}
    }

    #
    # Snack default progress callback
    #

    proc progressCallback {message fraction} {
	set w .snackProgressDialog

#	if {$fraction == 0.0} return
	if {$fraction == 1.0} {

	    # Task is finished close dialog

	    destroy $w
	    return
	}
	if {![winfo exists $w]} {

	    # Open progress dialog if not currently shown

	    toplevel $w
	    pack [label $w.l]
	    pack [canvas $w.c -width 200 -height 20 -relief sunken \
		    -borderwidth 2]
	    $w.c create rect 0 0 0 20 -fill black -tags bar
	    pack [button $w.b -text Stop -command "destroy $w.b"]
	    wm title $w "Please wait..."
	    wm transient $w .
	    wm withdraw $w
	    set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
		    - [winfo vrootx [winfo parent $w]]}]
	    set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
		    - [winfo vrooty [winfo parent $w]]}]
	    wm geom $w +$x+$y
	    wm deiconify $w
	    update idletasks
	} elseif {![winfo exists $w.b]} {

	    # User hit Stop button, close dialog
	    destroy $w
	    return -code error
	}
	switch -- $message {
	    "Converting rate" {
		set message "Converting sample rate..."
	    }
	    "Converting encoding" {
		set message "Converting sample encoding format..."
	    }
	    "Converting channels" {
		set message "Converting number of channels..."
	    }
	    "Computing pitch" {
		set message "Computing pitch..."
	    }
	    "Reading sound" {
		set message "Reading sound..."
	    }
	    "Writing sound" {
		set message "Writing sound..."
	    }
	    "Computing waveform" {
		set message "Waveform is being precomputed and\
			stored on disk..."
	    }
	    "Reversing sound" {
		set message "Reversing sound..."
	    }
	    "Filtering sound" {
		set message "Filtering sound..."
	    }
	}
	$w.l configure -text $message
	$w.c coords bar 0 0 [expr {$fraction * 200}] 20
	update
    }

    #
    # Convenience function to create dialog boxes, derived from tk_messageBox
    #

    proc makeDialogBox {toplevel args} {
	variable tkPriv
	
	set w tkPrivMsgBox
	upvar #0 $w data
	
	#
	# The default value of the title is space (" ") not the empty string
	# because for some window managers, a 
	#		wm title .foo ""
	# causes the window title to be "foo" instead of the empty string.
	#
	set specs {
	    {-default "" "" ""}
	    {-message "" "" ""}
	    {-parent "" "" .}
	    {-title "" "" " "}
	    {-type "" "" "okcancel"}
	}
	
	tclParseConfigSpec $w $specs "" $args
	
	if {![winfo exists $data(-parent)]} {
	    error "bad window path name \"$data(-parent)\""
	}
	
	switch -- $data(-type) {
	    abortretryignore {
		set buttons {
		    {abort  -width 6 -text Abort -under 0}
		    {retry  -width 6 -text Retry -under 0}
		    {ignore -width 6 -text Ignore -under 0}
		}
	    }
	    ok {
		set buttons {
		    {ok -width 6 -text OK -under 0}
		}
		if {![string compare $data(-default) ""]} {
		    set data(-default) "ok"
		}
	    }
	    okcancel {
		set buttons {
		    {ok     -width 6 -text OK     -under 0}
		    {cancel -width 6 -text Cancel -under 0}
		}
	    }
	    retrycancel {
		set buttons {
		    {retry  -width 6 -text Retry  -under 0}
		    {cancel -width 6 -text Cancel -under 0}
		}
	    }
	    yesno {
		set buttons {
		    {yes    -width 6 -text Yes -under 0}
		    {no     -width 6 -text No  -under 0}
		}
	    }
	    yesnocancel {
		set buttons {
		    {yes    -width 6 -text Yes -under 0}
		    {no     -width 6 -text No  -under 0}
		    {cancel -width 6 -text Cancel -under 0}
		}
	    }
	    default {
		error "bad -type value \"$data(-type)\": must be abortretryignore, ok, okcancel, retrycancel, yesno, or yesnocancel"
	    }
	}
	
	if {[string compare $data(-default) ""]} {
	    set valid 0
	    foreach btn $buttons {
		if {![string compare [lindex $btn 0] $data(-default)]} {
		    set valid 1
		    break
		}
	    }
	    if {!$valid} {
		error "invalid default button \"$data(-default)\""
	    }
	}
	
	# 2. Set the dialog to be a child window of $parent
	#
	#
	if {[string compare $data(-parent) .]} {
	    set w $data(-parent)$toplevel
	} else {
	    set w $toplevel
	}
	
	# 3. Create the top-level window and divide it into top
	# and bottom parts.
	
	#    catch {destroy $w}
	#    toplevel $w -class Dialog
	wm title $w $data(-title)
	wm iconname $w Dialog
	wm protocol $w WM_DELETE_WINDOW { }
	
	# Message boxes should be transient with respect to their parent so that
	# they always stay on top of the parent window.  But some window managers
	# will simply create the child window as withdrawn if the parent is not
	# viewable (because it is withdrawn or iconified).  This is not good for
	# "grab"bed windows.  So only make the message box transient if the parent
	# is viewable.
	#
	if { [winfo viewable [winfo toplevel $data(-parent)]] } {
	    wm transient $w $data(-parent)
	}    
	
	if {![string compare $::tcl_platform(platform) "macintosh"]} {
	    unsupported1 style $w dBoxProc
	}
	
	frame $w.bot
	pack $w.bot -side bottom -fill both
	if {[string compare $::tcl_platform(platform) "macintosh"]} {
	    $w.bot configure -relief raised -bd 1
	}
	
	# 4. Fill the top part with bitmap and message (use the option
	# database for -wraplength and -font so that they can be
	# overridden by the caller).
	
	option add *Dialog.msg.wrapLength 3i widgetDefault
	if {![string compare $::tcl_platform(platform) "macintosh"]} {
	    option add *Dialog.msg.font system widgetDefault
	} else {
	    option add *Dialog.msg.font {Times 18} widgetDefault
	}
	
	
	# 5. Create a row of buttons at the bottom of the dialog.
	
	set i 0
	foreach but $buttons {
	    set name [lindex $but 0]
	    set opts [lrange $but 1 end]
	    if {![llength $opts]} {
		# Capitalize the first letter of $name
		set capName [string toupper \
			[string index $name 0]][string range $name 1 end]
		set opts [list -text $capName]
	    }
	    
	    eval button [list $w.$name] $opts [list -command \
		[list set [namespace current]::tkPriv(button) $name]]
	    
	    if {![string compare $name $data(-default)]} {
		$w.$name configure -default active
	    }
	    pack $w.$name -in $w.bot -side left -expand 1 -padx 3m -pady 2m
	    
	    # create the binding for the key accelerator, based on the underline
	    #
	    set underIdx [$w.$name cget -under]
	    if {$underIdx >= 0} {
		set key [string index [$w.$name cget -text] $underIdx]
		bind $w <Alt-[string tolower $key]>  [list $w.$name invoke]
		bind $w <Alt-[string toupper $key]>  [list $w.$name invoke]
	    }
	    incr i
	}
	
	if {[string compare {} $data(-default)]} {
	    bind $w <FocusIn> {
		if {0 == [string compare Button [winfo class %W]]} {
		    %W configure -default active
		}
	    }
	    bind $w <FocusOut> {
		if {0 == [string compare Button [winfo class %W]]} {
		    %W configure -default normal
		}
	    }
	}
	
	# 6. Create a binding for <Return> on the dialog
	
	bind $w <Return> {
	 if {0 == [string compare Button [winfo class %W]]} {
	  if {$::tcl_version <= 8.3} {
	   tkButtonInvoke %W
	  } else {
	   tk::ButtonInvoke %W
	  }
	 }
	}
	
	# 7. Withdraw the window, then update all the geometry information
	# so we know how big it wants to be, then center the window in the
	# display and de-iconify it.
	
	wm withdraw $w
	update idletasks
	set x [expr {[winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
		- [winfo vrootx [winfo parent $w]]}]
	set y [expr {[winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
		- [winfo vrooty [winfo parent $w]]}]
	wm geom $w +$x+$y
	wm deiconify $w
	
	# 8. Set a grab and claim the focus too.
	
	set oldFocus [focus]
	set oldGrab [grab current $w]
	if {[string compare $oldGrab ""]} {
	    set grabStatus [grab status $oldGrab]
	}
	grab $w
	if {[string compare $data(-default) ""]} {
	    focus $w.$data(-default)
	} else {
	    focus $w
	}
	
	# 9. Wait for the user to respond, then restore the focus and
	# return the index of the selected button.  Restore the focus
	# before deleting the window, since otherwise the window manager
	# may take the focus away so we can't redirect it.  Finally,
	# restore any grab that was in effect.
	
	tkwait variable [namespace current]::tkPriv(button)
	
	catch {focus $oldFocus}
	destroy $w
	if {[string compare $oldGrab ""]} {
	    if {![string compare $grabStatus "global"]} {
		grab -global $oldGrab
	    } else {
		grab $oldGrab
	    }
	}
	return $tkPriv(button)
    }

    #
    # Snack level meter implemented as minimal mega widget
    #

    proc levelMeter {w args} {

	array set a [list \
		-oncolor red \
		-offcolor grey10 \
		-background black \
		-width 6 \
		-length 80 \
		-level 0.0 \
		-orient horizontal \
                -type log \
		]
	array set a $args

	# Widget specific storage

	namespace eval [namespace current]::$w {
	    variable levelmeter
	}
	upvar [namespace current]::${w}::levelmeter lm
	set lm(level) 0
	set lm(orient) $a(-orient)
	set lm(oncolor) $a(-oncolor)
	set lm(offcolor) $a(-offcolor)
	set lm(bg) $a(-background)
	set lm(type) $a(-type)
	if {[string match horiz* $lm(orient)]} {
	    set lm(height) $a(-width)
	    set lm(width)  $a(-length)
	} else {
	    set lm(height) $a(-length)
	    set lm(width)  $a(-width)
	}
	set lm(maxtime) [clock seconds]
	set lm(maxlevel) 0.0

	proc drawLevelMeter {w} {
            upvar [namespace current]::${w}::levelmeter lm

	    set c ${w}_levelMeter
	    $c configure -width $lm(width) -height $lm(height)
	    $c delete all

	    $c create rectangle 0 0 $lm(width) $lm(height) \
		    -fill $lm(oncolor) -outline ""
	    $c create rectangle 0 0 0 0 -outline "" -fill $lm(offcolor) \
		    -tag mask1
	    $c create rectangle 0 0 0 0 -outline "" -fill $lm(offcolor) \
		    -tag mask2
	    $c create rectangle 0 0 [expr $lm(width)-1] [expr $lm(height)-1] \
		    -outline $lm(bg)
	    if {[string match horiz* $lm(orient)]} {
		$c coords mask1 [expr {$lm(level)*$lm(width)}] 0 \
			$lm(width) $lm(height)
		$c coords mask2 [expr {$lm(level)*$lm(width)}] 0 \
			$lm(width) $lm(height)
		for {set x 5} {$x < $lm(width)} {incr x 5} {
		    $c create line $x 0 $x [expr $lm(width)-1] -fill black \
			    -width 2
		}
	    } else {
		$c coords mask1 0 0 $lm(width) \
			[expr {$lm(height)-$lm(level)*$lm(height)}]
		$c coords mask2 0 0 $lm(width) \
			[expr {$lm(height)-$lm(level)*$lm(height)}]
		for {set y 5} {$y < $lm(height)} {incr y 5} {
		    $c create line 0 [expr $lm(height)-$y] \
			    [expr $lm(width)-1] [expr $lm(height)-$y] \
			    -fill black -width 2
		}
	    }
	}

	proc levelMeterHandler {w cmd args} {
          upvar [namespace current]::${w}::levelmeter lm

          if {[string match conf* $cmd]} {
              switch -- [lindex $args 0] {
    	      -level {
		  set arg [lindex $args 1]
   		  if {$arg < 1} { set arg 1 }
	          if {$lm(type)=="linear"} {
                    set lm(level) [expr {$arg/32760.0}]
		  } else {
		    set lm(level) [expr {log($arg)/10.3972}]
		  }
		  if {[clock seconds] - $lm(maxtime) > 2} {
		    set lm(maxtime) [clock seconds]
		    set lm(maxlevel) 0.0
		  }
		  if {$lm(level) > $lm(maxlevel)} {
		    set lm(maxlevel) $lm(level)
		  }

		  if {[string match horiz* $lm(orient)]} {
		    set l1 [expr {5*int($lm(level)*$lm(width)/5)}]
		    set l2 [expr {5*int($lm(maxlevel)*$lm(width)/5)}]
		    ${w}_levelMeter coords mask1 $l2 0 \
			$lm(width) $lm(height)
		    ${w}_levelMeter coords mask2 [expr {$l2-5}] 0 \
			$l1 $lm(height)
		  } else {
		    set l1 [expr {5*int($lm(level)*$lm(height)/5)}]
		    set l2 [expr {5*int($lm(maxlevel)*$lm(height)/5)}]
		    ${w}_levelMeter coords mask1 0 0 $lm(width) \
			[expr {$lm(height)-$l2}]
		    ${w}_levelMeter coords mask2 0 [expr {$lm(height)-$l2+5}] \
			$lm(width) [expr {$lm(height)-$l1}]
		  }	 
	      }
	      -length {
		  if {[string match horiz* $lm(orient)]} {
		      set lm(width) [lindex $args 1]
		  } else {
		      set lm(height) [lindex $args 1]
		  }
		  drawLevelMeter $w
	      }
	      -width {
		  if {[string match horiz* $lm(orient)]} {
		      set lm(height) [lindex $args 1]
		  } else {
		      set lm(width)  [lindex $args 1]
		  }
		  drawLevelMeter $w
	      }
	      default {
		  error "unknown option \"[lindex $args 0]\""
	      }
	    }
	  } else {
	      error "bad option \"$cmd\": must be configure"
	  }
        }

	# Create a canvas where the widget is to be rendered

	canvas $w -highlightthickness 0

	# Replave the canvas widget command
	
	rename $w ${w}_levelMeter
	
	# Draw level meter

	drawLevelMeter $w

	# Create level meter widget command

	proc ::$w {cmd args} \
		"return \[eval snack::levelMeterHandler $w \$cmd \$args\]"

	return $w

    }
}
