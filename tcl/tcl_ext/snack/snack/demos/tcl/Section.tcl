#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

set width 300
set height 200
set start 0
set end 48000
set stipple ""
set winlen 256
set fftlen 512
set filename section.ps
set topfr 8000
set maxval 0.0
set minval -80.0
set skip 500
set atype FFT
set order 20
set wtype Hamming
option add *font {Helvetica 10 bold}

pack [ canvas .c -width 400 -height 250]
pack [ canvas .c2 -height 50 -width 400 -closeenough 5]
pack [ label .l -text "Drag markers with left mouse button"]
pack [ frame .f1] -pady 2
pack [ scale .f1.s1 -variable width -label Width -from 10 -to 400 \
    -orient horizontal -length 100 \
    -command [list .c itemconf sect -width ]] -side left
pack [ scale .f1.s2 -variable height -label Height -from 10 -to 250 \
    -orient horizontal -length 100 \
    -command [list .c itemconf sect -height ]] -side left
pack [ scale .f1.s3 -variable topfr -label "Top frequency" -from 1000 -to 8000 \
    -orient horizontal -length 100 -command [list .c itemconf sect -topfr ]] \
    -side left
pack [ scale .f1.s4 -variable maxval -label "Max value" -from 40 -to -40 \
    -orient horizontal -length 100 -command [list .c itemconf sect -maxvalue ]]\
    -side left
pack [ scale .f1.s5 -variable minval -label "Min value" -from -20 -to -100 \
    -orient horizontal -length 100 -command [list .c itemconf sect -minvalue ]]\
    -side left
pack [ scale .f1.s6 -variable skip -label "Skip" -from 50 -to 500 \
    -orient horizontal -length 100 -command [list .c itemconf sect -skip ]] \
    -side left

pack [ frame .f2i] -pady 2
pack [ label .f2i.lt -text "Type:"] -side left
tk_optionMenu .f2i.at atype FFT LPC
.f2i.at.menu entryconfigure 0 -command {.c itemconf sect -analysistype $atype;.f2i.e configure -state disabled;.f2i.s configure -state disabled}
.f2i.at.menu entryconfigure 1 -command {.c itemconf sect -analysistype $atype;.f2i.e configure -state normal;.f2i.s configure -state normal}
pack .f2i.at -side left

pack [ label .f2i.lo -text "order:"] -side left
entry .f2i.e -textvariable order -width 3

scale .f2i.s -variable order -from 1 -to 40 -orient horiz -length 60 -show no
pack .f2i.e .f2i.s -side left
.f2i.e configure -state disabled
.f2i.s configure -state disabled
bind .f2i.e <Key-Return> {.c itemconf sect -lpcorder $order}
bind .f2i.s <Button1-Motion> {.c itemconf sect -lpcorder $order}

tk_optionMenu .f2i.cm wtype Hamming Hanning Bartlett Blackman Rectangle
for {set i 0} {$i < 5} {incr i} {
  .f2i.cm.menu entryconfigure $i -command {.c itemconf sect -windowtype $wtype}
}
pack .f2i.cm -side left

pack [ label .f2i.lw -text "window:"] -side left
foreach n {32 64 128 256 512} {
    pack [ radiobutton .f2i.w$n -text $n -variable winlen -value $n \
	-command {.c itemconf sect -winlength $winlen}] -side left
}

pack [ frame .f3i] -pady 2
pack [ label .f3i.lf -text "FFT points:"] -side left
foreach n {64 128 256 512 1024} {
  pack [ radiobutton .f3i.f$n -text $n -variable fftlen -value $n \
      -command {.c itemconf sect -fft $fftlen}] -side left
}

set frame 1
pack [ frame .f2] -pady 2
pack [ checkbutton .f2.f -text Frame -variable frame \
    -command {.c itemconf sect -frame $frame}] -side left

foreach color {Black Red Blue} {
  pack [ radiobutton .f2.c$color -text $color -variable color -value $color \
      -command [list .c itemconf sect -fill $color]] -side left
}
set color Black

foreach {text value} {100% "" 50% gray50 25% gray25} {
  pack [ radiobutton .f2.$text -text $text -variable stipple -value $value \
      -command {.c itemconf sect -stipple $stipple}] -side left
}

pack [ frame .f3] -pady 2
pack [ button .f3.br -bitmap snackRecord -command Record -fg red] -side left
pack [ button .f3.bs -bitmap snackStop -command [list s stop]] -side left
pack [ label .f3.l -text "Load sound file:"] -side left
pack [ button .f3.b1 -text ex1.wav -command [list s read ex1.wav]] -side left
pack [ button .f3.b2 -text ex2.wav -command [list s read ex2.wav]] -side left

proc Record {} {
    s record
    after cancel [list catch {.f3.bs invoke}]
    after 10000  [list catch {.f3.bs invoke}]
}

pack [ frame .f4] -pady 2
pack [ label .f4.l -text "Generate postscript file:"] -side left
pack [ entry .f4.e -textvariable filename] -side left
pack [ button .f4.b -text Save -command {.c postscript -file $filename}] \
    -side left

pack [ button .bClose -text Close -command exit]

bind .c <1> [list initDrag %x %y]
bind .c <B1-Motion> [list Drag %x %y]

proc initDrag {x y} {
  set ::ox [.c canvasx $x]
  set ::oy [.c canvasy $y]
}

proc Drag {x y} {
  set x [.c canvasx $x]
  set y [.c canvasy $y]
  .c move current [expr $x - $::ox] [expr $y - $::oy]
  set ::ox $x
  set ::oy $y
}

snack::sound s -load ex1.wav

.c create section 200 125 -anchor c -sound s -height $height -width $width \
    -tags sect -frame $frame -debug 0 -start 9002 -end 12000

.c2 create spectrogram 0 0 -sound s -height 50 -width 400 -tags s
.c2 create line        5 0 5 50     -tags m1
.c2 create line        395 0 395 50 -tags m2

.c2 bind m1 <B1-Motion> {
    .c2 coords m1 [.c2 canvasx %x] 0 [.c2 canvasx %x] 100
    .c itemconf sect -start [expr int(16000 * [.c2 canvasx %x] / 600)]
}
.c2 bind m2 <B1-Motion> {
    .c2 coords m2 [.c2 canvasx %x] 0 [.c2 canvasx %x] 100
    .c itemconf sect -end [expr int(16000 * [.c2 canvasx %x] / 600)]
}
