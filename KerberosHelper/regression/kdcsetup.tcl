#!/usr/bin/expect  --
set timeout 10
spawn /usr/sbin/kdcsetup -a [lindex $argv 1] -p [lindex $argv 2] [lindex $argv 3]
expect {
    "Master Password" {
	set pw [lindex $argv 0]
	send "$pw\n"
	exp_continue
    } timeout {
	puts "\ntimeout\n"
	exit 1
    } eof {
    }
}

exit [lindex [wait] 3]
