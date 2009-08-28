#
# timings of libm functions
# under various extension techniques
#  1) Tcl's builtin expr math functions
#  2) Ffidl's ::ffidl::callout
#  3) SWIG wrappers
#  4) ::dll's ::dll::declare
#
lappend auto_path .
package require Ffidl 0.1
package require Ffidlrt 0.1

#
# you need to install the ::dll package, compile for your
# system, only Windows and Linux supported, and point this
# load at the result library.  no harm if you leave it as
# it is, the script catches any missing ::dll errors.
#
set nodll [catch {load ../../dll-1.0/unix/libdll10.so}]

#
# This Swig test extension is built by "make test" in
# the directory above.
#
set noswig [catch {load [::ffidl::find-lib mathswig]}]

#
# time, time, who's got the time.
#
proc timing {} {
    global nodll noswig
    set lib [::ffidl::find-lib m]
    set a [expr {1.23456789+0.0}]
    set b [expr {9.87654321+0.0}]
    proc nil {a} { set a }
    time {nil $a} 10000
    set t [time {nil $a} 10000]; puts "time for nil:		$t"

    ::ffidl::callout fficos {double} double [::ffidl::symbol $lib cos]
    if { ! $nodll } {::dll::declare $lib cos dllcos d d}
    if { ! $nodll } {set t [time {dllcos $a} 10000]; puts "time for dll cos:	$t"}
    if { ! $noswig} {set t [time {cos $a} 10000];    puts "time for swig cos:	$t"}
    set t [time {fficos $a} 10000];        puts "time for ffidl cos:	$t"
    set t [time {expr {cos($a)}} 10000];   puts "time for expr cos:	$t"

    ::ffidl::callout ffisqrt {double} double [::ffidl::symbol $lib sqrt]
    if { ! $nodll} {::dll::declare $lib sqrt dllsqrt d d}
    if { ! $nodll} {set t [time {dllsqrt $a} 10000]; puts "time for dll sqrt:	$t"}
    if { ! $noswig} {set t [time {sqrt $a} 10000];    puts "time for swig sqrt:	$t"}
    set t [time {ffisqrt $a} 10000];        puts "time for ffidl sqrt:	$t"
    set t [time {expr {sqrt($a)}} 10000];   puts "time for expr sqrt:	$t"

    ::ffidl::callout ffiatan2 {double double} double [::ffidl::symbol $lib atan2]
    if { ! $nodll} {::dll::declare $lib atan2 dllatan2 d d d}
    if { ! $nodll} {set t [time {dllatan2 $a $b} 10000]; puts "time for dll atan2:	$t"}
    if { ! $noswig} {set t [time {atan2 $a $b} 10000];    puts "time for swig atan2:	$t"}
    set t [time {ffiatan2 $a $b} 10000];        puts "time for ffidl atan2:	$t"
    set t [time {expr {atan2($a,$b)}} 10000];   puts "time for expr atan2:	$t"

    ::ffidl::callout ffiexp {double} double [::ffidl::symbol $lib exp]
    if { ! $nodll} {::dll::declare $lib exp dllexp d d}
    if { ! $nodll} {set t [time {dllexp $a} 10000]; puts "time for dll exp:	$t"}
    if { ! $noswig} {set t [time {exp $a} 10000];    puts "time for swig exp:	$t"}
    set t [time {ffiexp $a} 10000];        puts "time for ffidl exp:	$t"
    set t [time {expr {exp($a)}} 10000];   puts "time for expr exp:	$t"

    ::ffidl::callout ffilog {double} double [::ffidl::symbol $lib log]
    if { ! $nodll} {::dll::declare $lib log dlllog d d}
    if { ! $nodll} {set t [time {dlllog $a} 10000]; puts "time for dll log:	$t"}
    if { ! $noswig} {set t [time {log $a} 10000];    puts "time for swig log:	$t"}
    set t [time {ffilog $a} 10000];        puts "time for ffidl log:	$t"
    set t [time {expr {log($a)}} 10000];   puts "time for expr log:	$t"

    ::ffidl::callout ffifloor {double} double [::ffidl::symbol $lib floor]
    if { ! $nodll} {::dll::declare $lib floor dllfloor d d}
    if { ! $nodll} {set t [time {dllfloor $a} 10000]; puts "time for dll floor:	$t"}
    if { ! $noswig} {set t [time {floor $a} 10000];    puts "time for swig floor:	$t"}
    set t [time {ffifloor $a} 10000];        puts "time for ffidl floor:	$t"
    set t [time {expr {floor($a)}} 10000];   puts "time for expr floor:	$t"
}

timing
