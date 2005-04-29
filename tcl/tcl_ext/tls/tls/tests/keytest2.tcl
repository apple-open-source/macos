package require tls

set s [tls::socket 127.0.0.1 12300]
puts $s "A line"
flush $s
puts [join [tls::status $s] \n]
exit

