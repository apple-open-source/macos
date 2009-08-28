#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

set rate 16000
snack::sound s -rate $rate

# Length of FFT 
set n 1024
set type FFT

# Start recording, create polygon, and schedule a draw in 100 ms

proc Start {} {
  Stop
  set ::pos 0
  .c delete all
  .c create polygon -1 -1 -1 -1 -tags polar -fill green
  s record
  after 100 Draw
}

# Stop recording and updating the plot

proc Stop {} {
  s stop
  after cancel Draw
}

# Calculate spectrum and plot it

proc Draw {} {
  if {[s length] > $::n} {
    set ::pos [expr [s length] - $::n]
    set spec [s dBPowerSpectrum -start $::pos -fftlen $::n -winlen $::n \
	-analysistype $::type]
    set coords {}
    set f 0.0001
    foreach val $spec {
      set v [expr {6.282985 * log($f)/log(2.0)}]
      set a [expr {[winfo height .c]/214.0*($val+100)}]
      set x [expr {[winfo width .c]/2+$a*cos($v)}]
      set y [expr {[winfo height .c]/2+$a*sin($v)}]
      lappend coords $x $y
      set f [expr {$f + 16000.0 / $::n}]
    }
    eval .c coords polar $coords
  }
  after 10 Draw
  if {[s length -unit sec] > 20} Stop
}

# Create simple GUI

pack [ frame .f] -side bottom
pack [ button .f.b1 -bitmap snackRecord -command Start -fg red -width 40] \
    -side left
pack [ button .f.b2 -bitmap snackStop   -command Stop -width 40] -side left
pack [ radiobutton .f.b3 -text FFT -variable type -value FFT] -side left
pack [ radiobutton .f.b4 -text LPC -variable type -value LPC] -side left
pack [ canvas .c -width 300 -height 300 -bg black] -side top -expand true \
    -fill both
.c create text 150 150 -text "Polar spectrum plot of microphone signal" \
    -fill red
