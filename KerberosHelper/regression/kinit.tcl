#!/usr/bin/expect  --
set timeout 10
spawn /usr/bin/kinit [lindex $argv 0]
expect {
    "password" {
	set pw [lindex $argv 1]
	send "$pw\n"
	exp_continue
    } "Failed" {
	exit 1
    } timeout {
	puts "\ntimeout\n"
	exit 1
    } eof {
    }
}

exit [lindex [wait] 3]
