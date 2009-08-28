#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2
# Try to load optional file format handler
catch { package require snackogg }

snack::debug 0

set port 23654

proc Cmd { sock addr port } {
    global servsock msg

    set cmd [read $sock 4]
    switch $cmd {
	play {
	    set s [snack::sound -debug 0]
	    puts $sock $s ;# return token for this job
	    flush $sock
	    $s configure -channel $sock -guessproperties yes
	    $s play -command "close $sock; set msg idle;$s destroy"
	    set msg playing
	}
	stop {
	    set handle [gets $sock] ;# get token (sound name) and stop playback
	    catch { $handle stop }
	    catch { $handle destroy }
	    close $sock
	    set msg idle
	}
	exit {
	    close $sock
	    close $servsock
	    exit
	}
	default {
	    puts "Unknown command"
	}
    }
}

set servsock [socket -server Cmd $port]

# Make sure the server socket always is closed properly on exit

wm protocol . WM_DELETE_WINDOW {close $servsock; exit}

proc NewPort {} {
    global servsock port
    close $servsock
    set servsock [socket -server Cmd $port]
}

proc Pause {} {
  if {[snack::audio active] == 0} return
  snack::audio pause
  if {[.b.bp cget -relief] == "raised"} {
    .b.bp configure -relief sunken
    set ::msg paused
  } else {
    .b.bp configure -relief raised
    set ::msg playing
  }
}

proc Stop {} {
  snack::audio stop
  .b.bp configure -relief raised
  set ::msg idle
}

set msg idle
pack [frame .t]
pack [label .t.l1 -text Status:] -side left
pack [label .t.l2 -textvar msg -width 7] -side left

pack [frame .m]
pack [label .m.l -text Port] -side left
pack [entry .m.e -textvar port -width 6] -side left
pack [button .m.b -text Set -command NewPort] -side left

set gain [snack::audio play_gain]
pack [frame .b]
pack [button .b.bs -bitmap snackStop -command Stop] \
	-side left
pack [button .b.bp -bitmap snackPause -command Pause] -side left
pack [scale .b.s -show no -orient horiz -command {snack::audio play_gain} \
	-var gain] -side left

vwait forever
