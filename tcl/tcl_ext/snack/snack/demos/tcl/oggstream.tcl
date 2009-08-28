#!/bin/sh
# the next line restarts using tclsh \
exec wish8.4 "$0" "$@"

# Demonstration that sends a live recorded Ogg/Vorbis stream
# to the Snack sound server, aserver.tcl
# You will have to edit this script to set host and port
# to reflect were the server is run.

package require snack
package require snackogg

# Edit host and port here

set host ior.speech.kth.se
set port 23654

if [catch {set sock [socket $host $port]} res] {
    puts "Error: no aserver.tcl at $host:$port"
    exit
}

# Create sound object and attach it to the opened socket stream
sound s -channel $sock -channels 2 -rate 44100 -fileformat ogg

# Notify the server that a play operation is due
puts -nonewline $sock play

# Set desired bitrate
s config -nominalbitrate 32000

# Start recording
s record

# Keep the event loop alive, necessary for background recording
vwait forever
