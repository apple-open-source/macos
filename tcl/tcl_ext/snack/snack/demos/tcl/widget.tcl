#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2
package require http
if {[catch {package require tile} msg]} {
} else {
 namespace import -force ttk::button
}

option add *font {Helvetica 10 bold}
set home http://www.speech.kth.se/snack/
set version ""
set colors {#000 #006 #00B #00F #03F #07F #0BF #0FF #0FB #0F7 \
	    #0F0 #3F0 #7F0 #BF0 #FF0 #FB0 #F70 #F30 #F00}

snack::sound s1 -load ex1.wav
snack::sound s2

snack::menuInit
snack::menuPane File
snack::menuCommand File About { tk_messageBox -icon info -type ok \
    -title "About Widget Demo" -message \
    "Snack sound toolkit demonstration\n\n\
    Get the latest version at $home\n\n\
    Copyright (c) 1997-2001\n\nKare Sjolander"
}
snack::menuCommand File "Version" {
  set version "Looking up latest version at $home ..."
  catch {::http::geturl $home/$tcl_platform(platform).date -command CheckV}
}
snack::menuCommand File Mixer... snack::mixerDialog
snack::menuCommand File Quit exit

proc CheckV token {
  set ::version "Your version of Snack was released December 14, 2005. \
      Latest version released [::http::data $token]."
}

# Title label

pack [frame .t] -pady 5
pack [label .t.l -text "Snack Sound Toolkit Demonstrations"\
	-font {Helvetica 14 bold}]

# Basic sound handling

snack::createIcons
pack [frame .f0] -pady 5
pack [label .f0.l -text "Basic sound handling:"] -anchor w
label  .f0.time -text "0.00 sec" -width 10
button .f0.bp -image snackPlay -command {s2 play}
button .f0.bu -image snackPause -command {s2 pause}
button .f0.bs -image snackStop -command {s2 stop ; after cancel Timer}
button .f0.br -image snackRecord -command {s2 record ; Timer}
button .f0.bl -image snackOpen -command OpenSound -width 21 -height 21
button .f0.ba -image snackSave -command SaveSound -width 21 -height 21
pack .f0.bp .f0.bu .f0.bs .f0.br .f0.bl	.f0.ba .f0.time -side left

proc OpenSound {} {
    set filename [snack::getOpenFile]
    if {$filename == ""} return
    s2 read $filename
    .f0.time config -text [format "%.2f sec" [s2 length -unit seconds]]
}

proc SaveSound {} {
    set filename [snack::getSaveFile]
    if {$filename == ""} return
    s2 write $filename
}

proc Timer {} {
    .f0.time config -text [format "%.2f sec" [s2 length -unit seconds]]
    after 100 Timer
}

# Canvas item types

pack [canvas .c -width 680 -height 140 -highlightthickness 0] -pady 5
.c create text 0 0 -text "Waveform canvas item type:" -anchor nw
.c create waveform 0 20 -sound s1 -height 120 -width 250 -frame yes
.c create text 250 0 -text "Spectrogram canvas item type:" -anchor nw
.c create spectrogram 250 20 -sound s1 -hei 120 -wid 250 -colormap $colors
.c create text 480 0 -text "Spectrum section canvas item type:" -anchor nw
.c create section 500 20 -sound s1 -height 120 -width 180 -frame yes -start 8002 -end 10000 -minval -100

pack [frame  .f1] -pady 2
pack [label  .f1.l  -text "Waveform examples:" -wi 27 -anchor w] -side left
pack [button .f1.b1 -text "Simple" -command {Run MinWave.tcl}] -side left
pack [button .f1.b2 -text "See Code" -command {Browse MinWave.tcl}] -side left
pack [button .f1.b3 -text "Fancy" -command {Run Waveform.tcl}] -side left
pack [button .f1.b4 -text "See Code" -command {Browse Waveform.tcl}] -side left
pack [frame  .f2] -pady 2
pack [label  .f2.l  -text "Spectrogram examples:" -wi 27 -anchor w] -side left
pack [button .f2.b1 -text "Simple" -command {Run MinSpeg.tcl}] -side left
pack [button .f2.b2 -text "See Code" -command {Browse MinSpeg.tcl}] -side left
pack [button .f2.b3 -text "Fancy" -command {Run Spectrogram.tcl}] -side left
pack [button .f2.b4 -text "See Code" -command {Browse Spectrogram.tcl}] -side left
pack [frame  .f3] -pady 2
pack [label  .f3.l -text "Spectrum section examples:" -wi 27 -anchor w] -side left
pack [button .f3.b1 -text "Simple" -command {Run MinSect.tcl}] -side left
pack [button .f3.b2 -text "See Code" -command {Browse MinSect.tcl}] -side left
pack [button .f3.b3 -text "Fancy" -command {Run Section.tcl}] -side left
pack [button .f3.b4 -text "See Code" -command {Browse Section.tcl}] -side left
pack [frame  .f4] -pady 2
pack [label  .f4.l -text "Filter examples:"] -side left
pack [button .f4.b1 -text "Channel Map" -command {Run mapChan.tcl}] -side left
pack [button .f4.b2 -text "Echo" -command {Run echo.tcl}] -side left
pack [button .f4.b3 -text "Composite" -command {Run compose.tcl}] -side left
pack [button .f4.b4 -text "Generator" -command {Run generator.tcl}] -side left
pack [button .f4.b5 -text "Generator2" -command {Run generator2.tcl}] -side left
pack [button .f4.b6 -text "Notescale" -command {Run notescale.tcl}] -side left
pack [frame  .f5] -pady 2
pack [label  .f5.l -text "Sound tools:"] -side left
pack [button .f5.b1 -text "Simple" -command {Run cool.tcl}] -side left
pack [button .f5.b2 -text "dbrec" -command {Run dbrec.tcl}] -side left
pack [button .f5.b3 -text "record" -command {Run record.tcl}] -side left
pack [button .f5.b4 -text "xs" -command {Run xs.tcl demo}] -side left
pack [label  .f5.l2 -text "MP3 player:"] -side left
pack [button .f5.b5 -text "tomAmp" -command {Run tomAmp.tcl}] -side left
pack [label  .f5.l3 -text "Mixer:"] -side left
pack [button .f5.b6 -text "mixer" -command {Run mixer.tcl}] -side left
pack [frame  .f6] -pady 2
pack [label  .f6.l -text "Speech toys:"] -side left
pack [button .f6.b1 -text "Synthesis" -command {Run formant.tcl}] -side left
pack [button .f6.b2 -text "Pitch" -command {Run pitch.tcl}] -side left
pack [button .f6.b3 -text "Phonetogram" -command {Run phonetogram.tcl}] -side left
pack [button .f6.b4 -text "Vowel-space" -command {Run vowelspace.tcl}] -side left
pack [button .f6.b5 -text "Spectrum" -command {Run polarspec.tcl}] -side left
if {$::tcl_platform(platform) == "unix" || \
    $::tcl_platform(platform) == "windows"} {
  pack [label  .f6.l2 -text "Script compiler:"] -side left
  pack [button .f6.b6 -text "wrap" -command {Run wrap.tcl}] -side left
  pack [label .v -textvar version]
}

proc Run {script {demoFlag 0}} {
  set i [interp create]
  load {} Tk $i
  $i eval rename exit dontexit
  interp alias $i exit {} interp delete $i
  if {$demoFlag != 0} {
    $i eval set demoFlag $demoFlag
  }
  $i eval wm title . $script
  $i eval source $script
}

proc Browse file {
  set w .browse
  catch {destroy $w}
  toplevel $w
  wm title $w "View source: $file"
  
  pack [ button $w.b -text Close -command "destroy $w"] -side bottom
  pack [ frame $w.f] -fill both -expand yes
  text $w.f.t -width 60 -height 20 -setgrid true -wrap none
  $w.f.t config -xscrollcommand [list $w.f.xscroll set] -yscrollcommand [list $w.f.yscroll set]
  scrollbar $w.f.xscroll -orient horizontal -command [list $w.f.t xview]
  scrollbar $w.f.yscroll -orient vertical -command [list $w.f.t yview]
  pack $w.f.xscroll -side bottom -fill x
  pack $w.f.yscroll -side right -fill y
  pack $w.f.t -side left -fill both -expand yes
  
  if [catch {open $file} in] {
    set text $in
  } else {
    catch {set text [read $in]}
  }
  $w.f.t insert 1.0 $text
}
