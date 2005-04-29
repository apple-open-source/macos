#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# pop3 server for testing the client.
# Spawn this via pipe. Writes the port
# it is listening on to stdout. Takes
# the directory for its file system parts
# from the command line. Exits if stdin is
# closed.

# tmpdir  | set by caller
# testdir |
# logfile |

set modules [file dirname $testdir]
set popd    [file join $modules pop3d]
##set logfile [file join $tmpdir $logfile]
set log     [open $logfile w]

fconfigure $log -buffering none
proc log {txt} {global log ; puts $log $txt}
proc log__ {l t} {log "$l $t"}

fileevent stdin readable done
fconfigure stdin -blocking 0
proc done {} {
    gets stdin
    if {[eof stdin]} {
	global dboxdir
	log "shutdown through caller"
	catch {file delete -force $dboxdir}
	exit
    }
}


# Read server functionality

source [file join $popd pop3d.tcl]
source [file join $popd pop3d_dbox.tcl]
source [file join $popd pop3d_udb.tcl]

# Prevent log messages for now, or log into server log.

::log::lvCmdForall log__
#::log::lvSuppress info
#::log::lvSuppress notice
#::log::lvSuppress debug
#::log::lvSuppress warning


# Setup basic server

set srv [::pop3d::new]

$srv configure -port    0
$srv configure -auth    [set udb  [::pop3d::udb::new]]
$srv configure -storage [set dbox [::pop3d::dbox::new]]

# Configure the mail storage ...
# Directory, folders and mails .

set dboxdir [file join $tmpdir __dbox__]
if {[file exists $dboxdir]} {
    file delete -force $dboxdir
}
file mkdir $dboxdir
$dbox base $dboxdir
$dbox add         usr0
$dbox add         usr1

foreach m {10 20 30 40 50 60 70 80 90 100} {
    set f [open [file join $dboxdir usr0 $m] w]
    puts $f {
    }
    close $f

    set f [open [file join $dboxdir usr1 $m] w]
    puts $f {
    }
    close $f
}

set    f [open [file join $dboxdir usr0 15] w]
puts  $f {MIME-Version: 1.0
Content-Type: text/plain;
              charset="us-ascii"

Test1
Test2
Test3
Test4
x

.

--
Done}
close $f

# Configure the authentication ...

$udb add ak smash usr0
$udb add jh wooof usr1

# Start server ...

$srv up
set port [$srv cget -port]
puts  stdout $port
flush stdout

log "server up at $port"

vwait forever
log "reached infinity"
catch {file delete -force $dboxdir}
exit
