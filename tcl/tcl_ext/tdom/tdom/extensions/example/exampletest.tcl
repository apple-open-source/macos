#!tclsh

load ../../unix/libtdom0.7.5.so
load ./libexample1.0.0.so

set counter1 0
set counter2 0

proc eh1 {args} {
    global counter1

    incr counter1
}
proc eh2 {args} {
    global counter2

    incr counter2
}

set parser [expat]
$parser configure -elementstartcommand eh1 \
        -handlerset doMore \
        -elementstartcommand eh2
example $parser enable
tdom $parser enable

set fd [open ../../tests/data/books.xml]
$parser parse [read $fd]

puts "First tcl element start handler has counted the elements"
puts "(and 42 isn't a bad answer at all)"
puts "counter1: $counter1"
puts "\nSecond tcl element start handler has also counted the elements"
puts "(and should therefore report the same reasonable result)"
puts "counter2: $counter2"

puts "\nthe example C level handler set also counts the elements..."
puts "(and should maybe do some validation, instead of this ridiculous counting..)"
puts "example result: [example $parser getresult]"
set doc [tdom $parser getdoc]

set root [$doc documentElement]
puts "\n... but the second C level handler has done some serious work"
puts "DOM result tree root: [$root nodeName]"

puts "\nOK, reset the parser..."
$parser reset
puts "\nSome senseless fiddling with the result encoding"
puts [tdom $parser setResultEncoding]
puts [tdom $parser setResultEncoding iso8859-1]
puts [tdom $parser setResultEncoding]

puts "\nRemove the tdom handler set and parse again"
tdom $parser remove

seek $fd 0 start
$parser parse [read $fd]

puts "\nthe both tcl counters count further, thats OK"
puts "counter1: $counter1"
puts "counter2: $counter2"
puts "\nthe example counter is reseted because of the parser reset"
puts "example result: [example $parser getresult]"
puts "\nthe DOM tree created in the first parser run is still alive"
puts [$root childNodes]


