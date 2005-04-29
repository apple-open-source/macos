#!/bin/sh
# the next line restarts using wish \
exec wish8.4 "$0" "$@"

package require -exact snack 2.2

snack::sound s

set last 0
set server localhost:23654

proc Start {} {
    global sock server

    s record

    # Open binary socket connection to aserver.tcl

    foreach {host port} [split $server :] break
    set sock [socket $host $port]
    fconfigure $sock -translation binary
    if {$::tcl_version > 8.0} {
	fconfigure $sock -encoding binary
    }

    # Notify audio server that a play operation is due

    puts -nonewline $sock play

    # Send an AU file header to open the device correctly

    puts -nonewline $sock [s data -fileformat au]

    # Run SendData procedure in 200ms

    after 200 SendData
}

proc Stop {} {
    s stop
}

proc SendData {} {
    global last sock

    # There is new sound data to send

    if {[s length] > $last} {

	# Send audio data chunk in AU file format, "bigEndian"

	puts -nonewline $sock [s data -start $last -end -1 -fileformat raw\
		-byteorder bigEndian]
    }
    set last [s length]
    .l config -text Length:[s length]

    # User hit stop button, close down

    if ![snack::audio active] {
	set last 0
	close $sock
	return
    }
    after 300 SendData
}

pack [label .l -text "Length: 0"]

pack [frame .f1]
pack [label .f1.l -text "Server:"] -side left
pack [entry .f1.e -textvar server] -side left

pack [frame .f2]
pack [button .f2.a -bitmap snackRecord -command Start -wi 40 -he 20 -fg red] \
	-side left
pack [button .f2.b -bitmap snackStop -command Stop -wi 40 -he 20] -side left
pack [button .f2.c -bitmap snackPlay -command {s play} -wi 40 -he 20] \
	-side left
pack [button .f2.d -text Exit -command exit] -side left
