# Some utilities.

# converttomovietime --
#
#       Ex: converttomovietime .m 00:01:20.50 
#           one minute, 20 seconds, and 50 hundreds of a second.
# Arguments:
#       movie       the movie widget.
#       timehms     time in format HH:MM:SS.00
# Results:
#       the corresponding movie time.

proc converttomovietime {movie timehms} {

    array set timearr [$movie gettime]
    set time [split $timehms ":"]
    if {[llength $time] < 3} {
	error "Usage: \"converttomovietime moviePath HH:MM:SS.00\""
    }
    set outtime [expr 60*60*[lindex $time 0]] 
    set outtime [expr $outtime + 60 *[lindex $time 1]] 
    set outtime [expr $outtime + [lindex $time 2]] 
    return [expr int($outtime * $timearr(-movietimescale))] 
}

# converttohmstime --
#
#       Ex: converttohmstime .m 632718 => 0:17:34.53
# Arguments:
#       movie       the movie widget.
#       timemovie   the time in movies time scale.     
# Results:
#       time in format HH:MM:SS.00.

proc converttohmstime {movie timemovie} {

    array set timearr [$movie gettime]
    set hunsecs [format {%02i}    \
      [expr 100 * ($timemovie % $timearr(-movietimescale))/ \
      $timearr(-movietimescale)]]
    set totsecs [expr $timemovie/$timearr(-movietimescale)]
    set totmins [expr $totsecs/60]
    set tothours [expr $totmins/60]
    set secs [format {%02i} [expr $totsecs % 60]]
    set mins [format {%02i} [expr $totmins % 60]]
    
    return "${tothours}:${mins}:${secs}.${hunsecs}"
}
