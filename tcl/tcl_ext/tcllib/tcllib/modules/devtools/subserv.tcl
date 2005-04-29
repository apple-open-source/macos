# -*- tcl -*-
# Sub-servers, subservient
#
# Copyright (c) 2003 by Andreas Kupries <andreas_kupries@users.sourceforge.net>

# ####################################################################

# Code for the easy creation of sub-processes from a testsuite to
# perform some actions on behalf of it. General sub-processes and
# socket servers, the latter are based on "microserv.tcl".

# ####################################################################

namespace eval ::subserv {
    set here [file dirname [info script]] ; # To find muserv.tcl

    variable mPipe ; set mPipe ""
    variable mCtrl ; set mCtrol 0
    variable mLog  ; set mLog ""
}

package require log ; # tracing | tcllib

# ####################################################################
# API

# ::subserv::pipe --
#
#	Start a generic sub-process, controllable by its pipe.

proc ::subserv::pipe {pathToScriptFile} {
    log::log debug "subserv | pipe         | $pathToScriptFile"
    global tcl_platform
    switch -exact $tcl_platform(platform) {
	windows {return [open "|\"[info nameofexecutable]\" $pathToScriptFile" r+]}
	default {return [open "|[info nameofexecutable]     $pathToScriptFile" r+]}
    }
}

# ::subserv::exec --
#
#	Start a generic sub-process, via plain exec, asked to listen on port for
#	control commands.

proc ::subserv::exec {pathToScriptFile port} {
    global tcl_platform
    exec [info nameofexecutable] $pathToScriptFile $port &
    after 100
    return [socket localhost $port]
}

# ::subserv::muserv --
#
#	Create a micro server which can be run later.

proc ::subserv::muserv {pathToScriptFile ctrlport port responses} {
    global auto_path
    variable here

    log::log debug "subserv | muserv       | $pathToScriptFile $ctrlport $port [llength $responses]"

    catch {file delete -force $pathToScriptFile}
    set script [open $pathToScriptFile w]

    set ap $auto_path
    lappend ap [file dirname [file dirname [info script]]]

    puts $script ""
    puts $script "# -----------------------------------------------"
    puts $script "# Configuration of \"musub.tcl\""
    puts $script ""
    puts $script [list set auto_path $ap]
    puts $script [list set logfile   $pathToScriptFile.log]
    puts $script [list set port      $port]
    puts $script [list set responses $responses]
    puts $script [list set ctrlport  $ctrlport]
    puts $script ""
    puts $script "# -----------------------------------------------"
    puts $script ""

    set in [open [file join $here microserv.tcl] r]
    fcopy $in $script
    close $in
    set in [open [file join $here musub.tcl] r]
    fcopy $in $script
    close $in
    close $script
    return
}

# ::subserv::muservSpawn --
#
#	Create a micro server and run it immediately.

proc ::subserv::muservSpawn {pathToScriptFile port responses} {
    variable mPipe
    variable mCtrl

    log::log debug "subserv | muserv spawn | $pathToScriptFile $port [llength $responses]"

    set lsock    [socket -server ::subserv::muservCtrl 0]
    set ctrlport [lindex [fconfigure $lsock -sockname] end]

    log::log debug "subserv | muserv spawn | control on $ctrlport"

    muserv $pathToScriptFile $ctrlport $port $responses

    muservStop
    set mPipe [pipe $pathToScriptFile]

    log::log debug "subserv | muserv spawn | pipe on $mPipe"

    vwait ::subserv::mCtrl
    set     port [gets $mCtrl]

    log::log debug "subserv | muserv spawn | server waiting on $port"

    return $port
}

proc ::subserv::muservCtrl {thesock addr port} {
    variable mCtrl $thesock
    log::log debug "subserv | muserv ctrl  | $addr $port :: $mCtrl"
    return
}

# ::subserv::muservStop --
#
#	Stop a running micro server

proc ::subserv::muservStop {} {
    variable mPipe
    variable mCtrl

    if {$mPipe == {}} {return}

    log::log debug "subserv | muserv stop  | request"

    catch {close $mCtrl}
    catch {close $mPipe}

    log::log debug "subserv | muserv stop  | done"

    after 100 ; # sleep for a 1/10th second to make sure it is gone.
    set mPipe {}
    return
}

# ::subserv::muservLog --
#
#	Get a trace from the micro server

proc ::subserv::muservLog {} {
    variable mCtrl

    log::log debug "subserv | muserv log   | request"

    puts  $mCtrl ::muserv::gettrace
    flush $mCtrl

    log::log debug "subserv | muserv log   | collect"

    set res [list]
    while {1} {
	gets $mCtrl line
	log::log debug "subserv | muserv log   | __ $line"
	if {[string equal __EOTrace__ $line]} {break}
	lappend res $line
    }

    log::log debug "subserv | muserv log   | ok"
    return $res
}
