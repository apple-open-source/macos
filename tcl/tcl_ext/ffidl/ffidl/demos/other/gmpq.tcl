package provide Gmpq 2.0
package require Gmpz 2.0

#
# interface to arbitrary precison rational arithmetic
# 
namespace eval ::gmp:: {
    namespace export qadd qsub qmul qdiv qgcd qcmp qinv qneg qcvt qcanonicalize
}

proc ::gmp::qinit_set_str {qname str radix} {
    upvar $qname mpq
    foreach {num den} [split $str /] break
    if {[string length $den] == 0} { set den 1 }
    set mpz_num [binary format x[::ffidl::info sizeof mpz_struct]]
    set mpz_den [binary format x[::ffidl::info sizeof mpz_struct]]
    mpz_init_set_str mpz_num $num $radix
    mpz_init_set_str mpz_den $den $radix
    mpq_set_num mpq $mpz_num
    mpq_set_den mpq $mpz_den
    mpz_clear mpz_num
    mpz_clear mpz_den
    mpq_canonicalize mpq
}
proc ::gmp::qget_num {mpq} {
    set mpz [binary format x[::ffidl::info sizeof mpz_struct]]
    mpq_get_num mpz $mpq
    set mpz
}
proc ::gmp::qget_den {mpq} {
    set mpz [binary format x[::ffidl::info sizeof mpz_struct]]
    mpq_get_den mpz $mpq
    set mpz
}
proc ::gmp::qget_str {rad mpq} {
    if {$rad < 2 || $rad > 36} {
	error "radix $rad out of bounds, min 2, max 36"
    }
    set num [qget_num $mpq]
    set den [qget_den $mpq]
    set znum [zget_str $rad $num]
    set zden [zget_str $rad $den]
    mpz_clear num
    mpz_clear den
    if {[string compare $zden 1] == 0} {
	set r $znum
    }  else {
	set r $znum/$zden
    }
    set r
}
proc ::gmp::qunary {fn q1} {
    set mpq1 [binary format x[::ffidl::info sizeof mpq_struct]]
    set mpq2 [binary format x[::ffidl::info sizeof mpq_struct]]
    qinit_set_str mpq1 $q1 10
    mpq_init mpq2
    $fn mpq2 $mpq1
    set r [qget_str 10 $mpq2]
    mpq_clear mpq1
    mpq_clear mpq2
    set r
}
proc ::gmp::qbinary {fn q1 q2} {
    set mpq1 [binary format x[::ffidl::info sizeof mpq_struct]]
    set mpq2 [binary format x[::ffidl::info sizeof mpq_struct]]
    set mpq3 [binary format x[::ffidl::info sizeof mpq_struct]]
    qinit_set_str mpq1 $q1 10
    qinit_set_str mpq2 $q2 10
    mpq_init mpq3
    $fn mpq3 $mpq1 $mpq2
    set r [qget_str 10 $mpq3]
    mpq_clear mpq1
    mpq_clear mpq2
    mpq_clear mpq3
    set r
}
proc ::gmp::qbinaryi {fn q1 q2} {
    set mpq1 [binary format x[::ffidl::info sizeof mpq_struct]]
    set mpq2 [binary format x[::ffidl::info sizeof mpq_struct]]
    qinit_set_str mpq1 $q1 10
    qinit_set_str mpq2 $q2 10
    set r [$fn $mpq1 $mpq2]
    mpq_clear mpq1
    mpq_clear mpq2
    set r
}
proc ::gmp::qadd {i1 i2} { qbinary mpq_add $i1 $i2 }
proc ::gmp::qsub {i1 i2} { qbinary mpq_sub $i1 $i2 }
proc ::gmp::qmul {i1 i2} { qbinary mpq_mul $i1 $i2 }
proc ::gmp::qdiv {i1 i2} { qbinary mpq_div $i1 $i2 }
proc ::gmp::qcmp {i1 i2} { qbinaryi mpq_cmp $i1 $i2 }
proc ::gmp::qeq {i1 i2} { qbinaryi mpq_eq $i1 $i2 }
proc ::gmp::qneg {i1} { qunary mpq_neg $i1 }
proc ::gmp::qcvt {i1 rad1 rad2} {
    set mpq1 [binary format x[::ffidl::info sizeof mpq_struct]]
    qinit_set_str mpq1 $i1 $rad1
    set r [get_str $rad2 $mpq1]
    mpq_clear mpq1
    set r
}
