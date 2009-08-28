#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

set width 400
set height 100
set pps 300
set color black
set stipple ""
set filename waveform.ps
option add *font {Helvetica 10 bold}

pack [ canvas .c -width 600 -height 200]
pack [ label .l -text "Drag waveform with left mouse button"]
pack [ frame .f1] -pady 2
pack [ scale .f1.s1 -variable width -label Width -from 10 -to 600 \
	-orient hori -length 150 -command {.c itemconf wave -width }] -side left
pack [ scale .f1.s2 -variable height -label Height -from 10 -to 200 \
	-orient hori -length 150 -command {.c itemconf wave -height }] -side left
pack [ scale .f1.s3 -variable pps -label Pix/sec -from 10 -to 600 \
	-orient hori -length 150 -command {.c itemconf wave -pixels }] -side left

pack [ frame .f2] -pady 2
pack [ checkbutton .f2.z -text Zerolevel -variable zerol \
	-command {.c itemconf wave -zerolevel $zerol}] -side left
pack [ checkbutton .f2.f -text Frame -variable frame \
	-command {.c itemconf wave -frame $frame}] -side left

foreach f {Black Red Blue} {
    pack [ radiobutton .f2.c$f -text $f -variable color -value $f \
	    -command {.c itemconf wave -fill $color}] -side left
}

foreach {text value} {100% "" 50% gray50 25% gray25} {
  pack [ radiobutton .f2.$text -text $text -variable stipple -value $value \
      -command {.c itemconf wave -stipple $stipple}] -side left
}

pack [ frame .f3] -pady 2
pack [ button .f3.br -bitmap snackRecord -command Record -fg red] -side left
pack [ button .f3.bs -bitmap snackStop -command {s stop}] -side left
pack [ label .f3.l -text "Load sound file:"] -side left
pack [ button .f3.b1 -text ex1.wav -command {s read ex1.wav}] -side left
pack [ button .f3.b2 -text ex2.wav -command {s read ex2.wav}] -side left

proc Record {} {
    .c itemconf wave -pixelspersecond 300 -width 300
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

update

.c create waveform 300 100 -anchor c -sound s -height $height -tags wave -debug 0 -zerolevel 0
