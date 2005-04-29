#!/bin/sh
# the next line restarts using tclsh \
exec tclsh8.4 "$0" "$@"

if {[llength $argv] == 0} {
  puts {Usage: rplay.tcl file [host1:port] [host2:port] ...}
  exit
}

# Hostname of where an aserver.tcl is running
# Please edit here to set default host and port number

if {[llength $argv] == 1} {
  lappend argv localhost:23654
}

proc PlayFile {file hostport} {
  # Open the sound file as a binary file

  if [catch {set fd [open $file]} res] {
    puts $res
    exit
  }
  fconfigure $fd -translation binary
  if {$::tcl_version > 8.0} {
    fconfigure $fd -encoding binary
  }

  # Quick way to set both host and port

  foreach {host port} [split $hostport :] break

  # Open binary socket connection to aserver.tcl
    
  if [catch {set sock [socket $host $port]} res] {
    puts "Error: no aserver.tcl at $host,$port"
    exit
  }
  fconfigure $sock -translation binary -blocking 1
  if {$::tcl_version > 8.0} {
    fconfigure $sock -encoding binary
  }

  # Notify the server that a play operation is due
    
  puts -nonewline $sock play
  flush $sock

  # Get handle for this operation from the server.
  # Can be used to stop the operation later. (Not shown here.)

  set handle [gets $sock]
    
  # Write audio data as long as the server can accept more
    
  fileevent $sock writable "PlayHandler $fd $sock"
}

proc PlayHandler {fd sock} {
  set data [read $fd 10000]
  puts -nonewline $sock $data
  if [eof $fd] {
    close $fd
    close $sock
    exit
  }
}

# Use specified hosts

foreach host [lrange $argv 1 end] {
    PlayFile [lindex $argv 0] $host
}

vwait forever
