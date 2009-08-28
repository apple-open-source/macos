lappend auto_path .
package require Gmpq
namespace import ::gmp::q*
#
# binary operators
#
set ntests 0
set nfailures 0
foreach {op op1 op2 result} {
    qadd 1/2 1 3/2
    qadd 11111111111111111/3 11111111111111111/3 22222222222222222/3
    qsub 1 1 0
    qsub 11111111111111111 11111111111111111 0
    qmul 2 2 4
    qmul 22222222222222222 22222222222222222 493827160493827150617283950617284
    qdiv 4 2 2
    qdiv 44444444444444444 22222222222222222 2
    qcmp 2 1 1
    qcmp 1 2 -1
    qcmp 2 2 0
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
    qneg 1 -1
    qneg -1 1
} {
    if {[string compare [$op $op1] $result] != 0} {
	puts "\[$op $op1\] == [$op $op1] instead of $result"
	incr nfailures
    }
    incr ntests
}
#
# summary
#
puts "$nfailures failures in $ntests tests"
