lappend auto_path .
package require Gmpz
namespace import ::gmp::z*
#
# binary operators
#
set ntests 0
set nfailures 0
foreach {op op1 op2 result} {
    zadd 1 1 2
    zadd 11111111111111111 11111111111111111 22222222222222222
    zsub 1 1 0
    zsub 11111111111111111 11111111111111111 0
    zmul 2 2 4
    zmul 22222222222222222 22222222222222222 493827160493827150617283950617284
    zdiv 4 2 2
    zdiv 44444444444444444 22222222222222222 2
    zrem 4 2 0
    zrem 4 3 1
    zmod 4 3 1
    zgcd 22 33 11
    zcmp 2 1 1
    zcmp 1 2 -1
    zcmp 2 2 0
    zand 255 127 127
    zior 128 1 129
} {
    if {[string compare [$op $op1 $op2] $result] != 0} {
	puts "\[$op $op1 $op2\] == [$op $op1 $op2] instead of $result"
	incr nfailures
    }
    incr ntests
}
#
# unary operators
#
foreach {op op1 result} {
    zneg 1 -1
    zneg -1 1
    zcom 128 -129
    zabs -1 1
    zabs 1 1
    zsqrt 4 2
    zsqrt 10 3
    zsqrt 493827160493827150617283950617284 22222222222222222
} {
    if {[string compare [$op $op1] $result] != 0} {
	puts "\[$op $op1\] == [$op $op1] instead of $result"
	incr nfailures
    }
    incr ntests
}
#
# base conversion
#
foreach {op ibase obase result} {
    1010101 2 10 85
    85 10 2 1010101
    1777 8 10 1023
    1023 10 8 1777
} {
    if {[string compare [zcvt $op $ibase $obase] $result] != 0} {
	puts "\[zcvt $op $ibase $obase\] == [zcvt $op $ibase $obase] instead of $result"
	incr nfailures
    }
    incr ntests
}
#
# summary
#
puts "$nfailures failures in $ntests tests"
