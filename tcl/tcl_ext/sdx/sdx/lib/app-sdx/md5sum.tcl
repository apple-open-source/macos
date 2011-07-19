# calculate MD5 message digests

package require md5

if {[llength $argv] < 1} {
	puts stderr "  Usage: md5sum files..."
    exit 1
}

foreach file $argv {
	if {[catch {
		set fd [open $file]
		fconfigure $fd -trans binary
		set sum [md5::md5 [read $fd]]
		close $fd
		puts "$sum  $file"
	} msg]} {
		puts "md5sum: $file: $msg"
	}
}
