#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

# pop3 client, loaded with sequence of operations
# to perform.

set modules [file dirname $testdir]
set pop     [file join $modules pop3]

# Read client functionality

source [file join $testdir pop3.tcl]

proc log {code {payload {}}} {
    puts stdout [list $code $payload]
    flush stdout
    return
}

proc res {fail msg} {log res [list $fail $msg]}
proc wait {} {while {[gets stdin line] < 0} {}}

# Run the provided operations ...
# Mini CPU ...

set chan {}
set fail 0

foreach op $ops {
    foreach {cmd ca} $op break
    switch -exact -- $cmd {
	wait {wait}
	poke {
	    res 0 $::pop3::state($chan)
	}
	open {
	    foreach {user passwd} $ca break
	    set  fail [catch {set chan [::pop3::open localhost $user $passwd $port]} msg]
	    res $fail $msg
	}
	close {
	    set  fail [catch {::pop3::close $chan} msg]
	    res $fail $msg
	}
	status {
	    set  fail [catch {::pop3::status $chan} msg]
	    res $fail $msg
	}
	top {
	    foreach {msg n} $ca break
	    set  fail [catch {::pop3::top $chan $msg $n} msg]
	    res $fail $msg
	}
	retrieve {
	    foreach {start end} $ca break
	    if {$end == {}} {set end -1}
	    set  fail [catch {::pop3::retrieve $chan $start $end} msg]
	    res $fail $msg
	}
	delete {
	    foreach {start end} $ca break
	    if {$end == {}} {set end -1}
	    set  fail [catch {::pop3::delete $chan $start $end} msg]
	    res $fail $msg
	}
	list {
	    foreach {msg} $ca break
	    set  fail [catch {::pop3::list $chan $msg} msg]
	    res $fail $msg
	}
	uidl {
	    foreach {msg} $ca break
	    set  fail [catch {::pop3::uidl $chan $msg} msg]
	    res $fail $msg
	}
	last {
	    set  fail [catch {::pop3::last $chan} msg]
	    res $fail $msg
	}
    }
    if {$fail} break
}

# Wait for last call from control and then exit.

log done
wait
exit
