#!/bin/sh
# the next line restarts using wish \
exec wish8.3 "$0" "$@"

package require -exact snack 2.2

# there is no way (?) to find out from Tk if we can display UNICODE IPA
# but it seems to be standard on windows installations
if {[string match windows $tcl_platform(platform)]} {set UNICODE_IPA 1}

switch $tcl_platform(platform) {
 windows {
  proc milliseconds { } {clock clicks}
 }
 unix {
  proc milliseconds { } {expr {[clock clicks]/1000}}
 }
}


set vowels(sw) {
 O: u      300 600 2350 3250
 O  \u028a 350 700 2600 3200
 Å: o      400 700 2450 3250
 Å  \u0254 500 850 2550 3250
 A: \u0251 600 950 2550 3300
 A  a      750 1250 2500 3350
 I: i      250 2200 3150 3750
 I  \u026a 350 2150 2750 3500
 E: e      350 2250 2850 3550
 E/Ä \u025b 500 1900 2550 3350
 Ä3 \ue6   650 1700 2500 3450
 Y: y      250 2050 2700 3300
 Y  \u028f 300 2000 2400 3250
 Ö: \uf8   400 1750 2300 3350
 Ö  \u153  550 1550 2450 3300
 Ö3 ""     550 1150 2450 3250
 U: \u0289 300 1650 2250 2250
 U  \u0275 450 1050 2300 3300
}

set vowels(us) {
 i i 280 2250 2890 {}
 I \u026a 400 1920 2560 {}
 E \u025b 550 1770 2490 {}
 @ \u00e 6690 1660 2490 {}
 A \u0251 710 1100 2540 {}
 > \u0254 590 880 2540 {}
 U \u028a 450 1030 2380 {}
 u u 310 870 2250 {}
}

set vowels(lang) us
 
proc vok4Create {w {wid 200} {hei 200}} {
 upvar #0 $w a
 frame $w -width $wid -height $hei
 pack [canvas $w.c -bg black] -fill both -expand 1
 pack propagate $w 0
 set a(xm) 20
 set a(ym) 20
 set a(F10) 800
 set a(F11) 200
 set a(F20) 2300
 set a(F21) 500
 
 $w.c create line 0 0 0 0 -fill white -tags axes -arrow both
 $w.c create text 0 0 -anchor e -text F2 -fill yellow -tags ylabel
 $w.c create text 0 0 -anchor n -text F1 -fill yellow -tags xlabel
 menu $w.m -tearoff 0
 $w.m add radiobutton -variable vowels(lang) -value sw \
   -command [list vok4Config $w] -label "Swedish vowels (after Fant)"
 $w.m add radiobutton -variable vowels(lang) -value us \
   -command [list vok4Config $w] -label "American vowels (after Ladefoged)"
 $w.m add radiobutton -variable vowels(lang) -value NIL \
   -command [list vok4Config $w] -label "Don't display vowels"
 
 # trailInit $w 10
 
 bind $w.c <ButtonPress-1> "vok4Move $w %x %y;Play"
 bind $w.c <B1-Motion> [list vok4Move $w %x %y]
 bind $w.c <ButtonRelease-1> "Stop"
 bind $w.c <Configure> [list vok4Config $w %w %h]
 bind $w.c <ButtonPress-3> [list tk_popup $w.m %X %Y]
 return $w
}

proc vok4Config {w {wid -1} {hei -1}} {
 upvar #0 $w a
 
 if {$wid==-1} {
  set wid $a(width)
  set hei $a(height)
 } else {
  set a(width) $wid
  set a(height) $hei
 }
 set a(x0) $a(xm)
 set a(x1) [expr $wid-$a(xm)]
 set a(y0) [expr $hei-$a(ym)]
 set a(y1) $a(ym)
 $w.c coords axes $a(x0) $a(y1) \
   $a(x1) $a(y1) $a(x1) $a(y0)
 $w.c coords ylabel $a(x0) $a(y1)
 $w.c coords xlabel $a(x1) $a(y0)
 
 $w.c delete sym

 set lang $::vowels(lang)
 if [info exists ::vowels($lang)] {
  foreach {ascii uni f1 f2 f3 f4} $::vowels($lang) {
   if [info exists ::UNICODE_IPA] {set sym $uni} else {set sym $ascii}
   set x [expr {$a(x0)+($a(x1)-$a(x0))*($f2-$a(F20))*1.0/($a(F21)-$a(F20))}]
   set y [expr {$a(y0)+($a(y1)-$a(y0))*($f1-$a(F10))*1.0/($a(F11)-$a(F10))}]
   $w.c create text $x $y -font "times 16" -anchor c -text $sym -fill gray -tags sym
  }
 }
}

proc vok4Move {w x y} {
# puts [info level 0]
 upvar #0 $w a
 
 set f1 [expr {int($a(F10)+($a(F11)-$a(F10))*($y-$a(y0))*1.0/($a(y1)-$a(y0)))}]
 set f2 [expr {int($a(F20)+($a(F21)-$a(F20))*($x-$a(x0))*1.0/($a(x1)-$a(x0)))}]
 set ::v(f1) $f1
 set ::v(f2) $f2
 Config
 set a(curx) $x
 set a(cury) $y
 #  trailUpdate $w
 return ""
}

proc updatePreview {} {
 $::v(pGen) configure \
   $::v(g,freq) $::v(g,ampl) [expr 0.01*$::v(g,shape)] $::v(g,type) 1024
 $::v(pF1) configure $::v(f1) $::v(b1)
 $::v(pF2) configure $::v(f2) $::v(b2)
 $::v(pF3) configure $::v(f3) $::v(b3)
 $::v(pF4) configure $::v(f4) $::v(b4)

 preview2 copy s
 preview2 filter $::v(pAll)
 preview1 copy s
 preview1 filter $::v(pGen)

 after cancel updatePreview
 if {$::v(on) && $::v(g,type)=="noise"} {
  after 100 updatePreview
 }
}

proc Config {args} {
 $::v(Gen) configure \
   $::v(g,freq) $::v(g,ampl) [expr 0.01*$::v(g,shape)] $::v(g,type) -1
 $::v(F1) configure $::v(f1) $::v(b1)
 $::v(F2) configure $::v(f2) $::v(b2)
 $::v(F3) configure $::v(f3) $::v(b3)
 $::v(F4) configure $::v(f4) $::v(b4)
 updatePreview
}

proc Play {} {
 set ::v(on) 1
 s stop
 s play -filter $::v(All)
 updatePreview
 set ::v(tstart) [milliseconds]
 #  updateTracks
 .f1.b config -relief sunken
}

proc Stop {} {
 s stop
 set ::v(on) 0
 .f1.b config -relief raised
}

proc Load {} {
 set file [snack::getOpenFile]
 if {$file != ""} {s read $file}
}

proc updateTracks {} {
 set tt 50
 set now [milliseconds]
 set then $::v(tstart)
 set dt [expr 1.0*([milliseconds]-$::v(tstart))]
 #set ::v(g,freq) [expr 100+100*(1.0*$dt/$tt)*exp(-$dt/$tt)]
 set ::v(g,freq) [expr {100+2*cos(2*3.1415*$dt/$tt)}]

 Config

 if $::v(on) {
  after 50 updateTracks
 }
}

proc labeledScale {w args} {
 array set a {-valwidth 4 -labwidth 8}
 array set a $args
 catch {set a(-text) $a(-label)}

 frame $w
 pack [label $w.l -anchor w -width $a(-labwidth)] -side left
 foreach opt {-text -bg -width -font} {
  if [info exists a($opt)] {$w.l config $opt $a($opt)}
 }
 pack [scale $w.s -showvalue 0 -bd 1 -width 10] -side left -expand 1 -fill x
 pack [label $w.v -textvariable $a(-variable) -width $a(-valwidth) -anchor w] -side left
 foreach opt {-bg -font} {
  if [info exists a($opt)] {$w.v config $opt $a($opt)}
 }
 foreach opt {-length -bg -from -to -variable -orient -resolution -command} {
  if [info exists a($opt)] {$w.s config $opt $a($opt)}
 }
 return $w
}

proc About {} {
 set w .about
 catch {destroy $w}
 toplevel $w
 wm title $w "About: Formant Synthesis Demo"
 set text " This application demonstrates formant-based synthesis
 of vowels in real time, in the spirit of Gunnar Fant's 
 Orator Verbis Electris (OVE-1) synthesizer of 1953.

 Set source and filter parameters at the top. Click and 
 drag in the \"vowel space\" to hear the vowels. 
 Right-click to select target language for vowel symbols.
 
 Power spectrum of source (red) and output signal (green) are 
 to the right, waveforms are displayed at the bottom.
 
 The source type \"sampled\" will use a sound file 
 containing a single period of a waveform as voice source.

 Copyright © 2000 Jonas Beskow
 Centre for Speech Technology
 KTH, Stockholm"

 label $w.l -text $text -relief groove -bd 2
 button $w.b -text OK -command [list set about_done 1]
 pack $w.l -side top -expand 1 -fill both -padx 5 -pady 5
 pack $w.b -side top -padx 5 -pady 5
 if [catch {::tk::PlaceWindow $w center}] {
  wm geometry $w +[winfo rootx .]+[winfo rooty .]
 }
 vwait about_done
 destroy $w
}


wm title . "Formant Synthesizer Demo"
wm resizable . 0 0

# Menu bar

menu .m
.m add cascade -label File -menu [menu .m.file -tearoff 0]
.m add cascade -label Help -menu [menu .m.help -tearoff 0]
.m.file add command -label "Load source waveform..." -command Load
.m.file add separator
.m.file add command -label Exit -command exit
.m.help add command -label About... -command About
. configure -menu .m

# Generator GUI

frame .f1 -relief groove -bd 2
grid .f1 -row 0 -column 0 -sticky news -padx 5 -pady 5
label .f1.l -text Source -bg red -anchor w
tk_optionMenu .f1.gt v(g,type) rectangle triangle sine sampled noise
button .f1.b -bitmap snackPlay -command Play
button .f1.c -bitmap snackStop -command Stop

labeledScale .f1.gf -label "Freq." -variable v(g,freq) -from 0.0 -to 1000 -resolution 1.0 -orient horiz -command Config
labeledScale .f1.ga -label "Ampl." -variable v(g,ampl) -from 0.0 -to 6000 -resolution 1.0 -length 160 -orient horiz -command Config
labeledScale .f1.gs -label "Shape" -variable v(g,shape) -from 0.0 -to 100 -resolution 1.0 -length 160 -orient horiz -command Config

grid .f1.l .f1.gt .f1.b .f1.c -sticky we -padx 5
grid .f1.gf -columnspan 4 -sticky we
grid .f1.ga -columnspan 4 -sticky we
grid .f1.gs -columnspan 4 -sticky we
grid columnconfigure .f1 0 -weight 1
grid rowconfigure .f1 4 -weight 1
# Formant filter GUI

frame .f2 -relief groove -bd 2
grid .f2 -row 0 -column 1 -sticky news -padx 5 -pady 5
label .f2.l -text "Formants" -bg green -anchor w
grid .f2.l -columnspan 5 -sticky we -padx 5 -pady 5
label .f2.lf -text "Frequency" -anchor w
label .f2.lfu -text "Hz "
label .f2.lb -text "Bandwidth" -anchor w
label .f2.lbu -text "Hz "
grid .f2.lf -row 1 -column 1 -sticky w
grid .f2.lfu -row 1 -column 2 -sticky w
grid .f2.lb -row 1 -column 3 -sticky w
grid .f2.lbu -row 1 -column 4 -sticky w

for {set i 1} {$i<=4} {incr i} {
 label .f2.l0$i -text F$i -width 2
 scale .f2.f$i -variable v(f$i) -from 0 -to 5000 -resolution 1.0 -orient horiz -command Config -showvalue 0 -bd 1 -width 10
 label .f2.l1$i -textvariable v(f$i) -anchor w -width 4
 scale .f2.b$i -variable v(b${i}) -from 1.0 -to 500 -resolution 1.0 -orient horiz -command Config -showvalue 0 -bd 1 -width 10 -length 80
 label .f2.l2$i -textvariable v(b$i) -anchor w -width 3
 grid .f2.l0$i .f2.f$i .f2.l1$i .f2.b$i .f2.l2$i -sticky news
}
grid columnconfigure .f2 1 -weight 1

set vokh 250
set vokw 275

# Vowel space

vok4Create .voc $vokw $vokh
grid .voc -row 1 -column 0 -sticky news

# Spectrum section preview

snack::sound preview1
snack::sound preview2

set secw $vokw
set sech $vokh

canvas .c2 -bg black -height 100 -width $secw
grid .c2 -row 1 -column 1  -sticky news
.c2 create section 0 0 -sound preview1 -fill red -height $sech -topfrequency 4000 -width $secw -analysistype lpc -tags sect -maxvalue 30
.c2 create section 0 0 -sound preview2 -fill green -height $sech -topfrequency 4000 -width $secw -analysistype lpc -tags sect -maxvalue 30

foreach freq {1 2 3 4} {
 set x [expr {$freq*$secw/4.0}]
 .c2 create line $x 0 $x $sech -fill #999999
 .c2 create text $x 0 -anchor ne -text $freq -fill #999999
}
.c2 create text 0 0 -anchor nw -text kHz -fill #999999
.c2 raise sect

# Waveforms preview

set wavw 550
set wavh 90

canvas .c1 -bg black -height 100 -width $wavw
grid .c1 -row 2 -columnspan 2 -sticky news
.c1 create waveform 0 50 -anchor w -sound preview1 -fill red  -height $wavh -pixelspersecond 16000
.c1 create waveform 0 50 -anchor w -sound preview2 -fill green  -height $wavh -pixelspersecond 16000

# Default values

set v(f1) 500
set v(b1) 50
set v(f2) 1500
set v(b2) 75
set v(f3) 2500
set v(b3) 100
set v(f4) 3500
set v(b4) 150
set v(g,freq) 75
set v(g,ampl) 2500
set v(g,shape) 10
set v(g,type) rectangle

# Create the filters
set v(F1) [snack::filter formant $v(f1) $v(b1)]
set v(F2) [snack::filter formant $v(f2) $v(b2)]
set v(F3) [snack::filter formant $v(f3) $v(b3)]
set v(F4) [snack::filter formant $v(f4) $v(b4)]
set v(Gen) [snack::filter generator $v(g,freq)]
set v(All) [snack::filter compose $v(Gen) $v(F1) $v(F2) $v(F3) $v(F4)]

# Create spearate filters for the preview
set v(pF1) [snack::filter formant $v(f1) $v(b1)]
set v(pF2) [snack::filter formant $v(f2) $v(b2)]
set v(pF3) [snack::filter formant $v(f3) $v(b3)]
set v(pF4) [snack::filter formant $v(f4) $v(b4)]
set v(pGen) [snack::filter generator $v(g,freq)]
set v(pAll) [snack::filter compose $v(pGen) $v(pF1) $v(pF2) $v(pF3) $v(pF4)]

set v(on) 0

snack::sound s
snack::createIcons

set samples {135 1477 969 -524 -784 314 781 -19 -543 70 696 366 -141 154 694 484 -122 -179 199 290 136 229 429 293 0 48 326 321 44 -15 210 296 137 99 256 254 82 193 625 800 497 234 346 478 354 264 411 516 420 412 628 724 524 389 563 714 557 378 477 608 476 320 450 658 598 395 380 545 628 558 486 484 461 393 383 446 464 413 399 459 520 559 612 668 670 618 569 536 481 390 312 278 255 224 199 176 152 148 158 119 0 -130 -209 -275 -405 -594 -777 -922 -1046 -1187 -1420 -1822 -2267 -2179}
s length [llength $samples]
for {set i 0} {$i<[s length]} {incr i} {
 s sample $i [lindex $samples $i]
}

trace variable v(g,type) w Config
