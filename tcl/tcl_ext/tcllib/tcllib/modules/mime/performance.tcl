#! /bin/sh
# -*- tcl -*- \
exec tclsh "$0" ${1+"$@"}

#package require mime
source ./mime.tcl 

proc construct_item_with_attachment size {
    set message_token [mime::initialize -canonical text/plain \
            -string "This is a first part."]
    set attachment_body [string repeat abcd\n [expr {$size / 5}]]
    set attachment_token [mime::initialize \
            -canonical application/octet-stream \
            -string $attachment_body]
    set multi_token [mime::initialize -canonical multipart/mixed \
            -parts [list $message_token $attachment_token]]

    set packaged [mime::buildmessage $multi_token]
    mime::finalize $multi_token
    return $packaged
}

proc small_test size {
    set item [construct_item_with_attachment $size]
    #puts $item
    set length [string length $item]
    set result [time {mime::finalize [mime::initialize \
                       -string $item]} 1]

    puts "$size ($length):  $result"
}

small_test 800000
small_test 1000000
small_test 1500000
small_test 2500000
small_test 5000000


small_test 1000
small_test 10000
small_test 50000
small_test 100000
small_test 200000
small_test 400000


exit
foreach func [profiler::sortFunctions totalRuntime] {
    if { [lindex $func 1] > 0 } {
	puts [profiler::print [lindex $func 0]]
    }
}
exit

set fp [open /tmp/msgdump r]
set message [read $fp]
close $fp

set curpos 0
set next_EOL -1
set msg_EOF 0
set msg_size [string length $message]

proc doforeach {} {
    global message

    set cnt 0
    foreach line [split $message "\n"] {
	incr cnt
    }
    puts "doforeach $cnt lines"    

}

proc dolindex {} {
    global message 
    set cnt 0
    set lmsg [split $message "\n"]
    set len [llength $lmsg]
    for {set cnt 0} { $cnt < $len } {incr cnt} {
	set line [lindex $lmsg $cnt]
    }

    puts "dolindex $cnt lines"    
    
}

proc getnextline {} {
    global message
    global curpos
    global next_EOL
    global msg_EOF
    global msg_size

   if { $msg_EOF } {
	error "End-Of-Message reached"
    }

    set next_EOL [string first "\n" $message $curpos]

    if { $next_EOL == -1 } {
	set next_EOL $msg_size	
    }

    set msg_EOF [expr {$next_EOL == $msg_size}]

    set line [string range $message $sp $next_EOL] 
    set curpos [incr next_EOL]

}

proc dogetnext {} {
    global message
    global curpos
    global next_EOL
    global msg_EOF
    global msg_size

    set curpos 0
    set next_EOL -1
    set msg_EOF 0
    set msg_size [string length $message]

    set cnt 0
    while { !$msg_EOF } {
	getnextline
	incr cnt
    }

    puts "dogetnext $cnt lines"    
}

set res [time doforeach 10]
puts $res
set time1 [lindex $res 0]

set res [time dolindex 10]
puts $res
set time2 [lindex $res 0]
puts [expr $time2.0 / $time1.0 ]
