package require starsync

set r [starsync::reply]

lassign $argv log
if {$log ne "" && [file writable $log]} {
  set time [clock format [clock seconds] -format {%Y/%m/%d %T}]
  if {[catch { set env(REMOTE_ADDR) } addr]} { set addr - }
  if {[catch { set env(REQUEST_URI) } uri]} { set uri - }
  set fd [open $log a]
  puts $fd [linsert $r 0 $addr $time $uri]
  close $fd
}
