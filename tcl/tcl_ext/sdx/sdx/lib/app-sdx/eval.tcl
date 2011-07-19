
#tclLog argv=$argv
#tclLog argv=[llength $argv]

if {[llength $argv] == 0} {
    puts stderr "usage: eval arg ?arg ...?"
    exit 1
}

set ret [eval $argv]
if {$ret != ""} {
    puts stdout $ret
}
