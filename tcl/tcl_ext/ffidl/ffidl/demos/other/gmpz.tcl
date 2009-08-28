package provide Gmpz 2.0
package require Gmp 2.0
#
# interface to arbitrary precison integer arithmetic
# 
namespace eval ::gmp:: {
    namespace export zadd zsub zmul zdiv zrem zmod zgcd zcmp zand zior zcom zneg zabs zsqrt zcvt
}

proc ::gmp::zget_str {rad mpz} {
    if {$rad < 2 || $rad > 36} {
	error "radix $rad out of bounds, min 2, max 36"
    }
    set mpzo [binary format x[expr {[mpz_sizeinbase $mpz $rad]+2}]]
    mpz_get_str mpzo $rad $mpz
}
proc ::gmp::zunary {fn i1} {
    set mpz1 [binary format x[::ffidl::info sizeof mpz_struct]]
    set mpz2 [binary format x[::ffidl::info sizeof mpz_struct]]
    mpz_init_set_str mpz1 $i1 10
    mpz_init mpz2
    $fn mpz2 $mpz1
    set r [zget_str 10 $mpz2]
    mpz_clear mpz1
    mpz_clear mpz2
    set r
}
proc ::gmp::zbinary {fn i1 i2} {
    set mpz1 [binary format x[::ffidl::info sizeof mpz_struct]]
    set mpz2 [binary format x[::ffidl::info sizeof mpz_struct]]
    set mpz3 [binary format x[::ffidl::info sizeof mpz_struct]]
    mpz_init_set_str mpz1 $i1 10
    mpz_init_set_str mpz2 $i2 10
    mpz_init mpz3
    $fn mpz3 $mpz1 $mpz2
    set r [zget_str 10 $mpz3]
    mpz_clear mpz1
    mpz_clear mpz2
    mpz_clear mpz3
    set r
}
proc ::gmp::zbinaryi {fn i1 i2} {
    set mpz1 [binary format x[::ffidl::info sizeof mpz_struct]]
    set mpz2 [binary format x[::ffidl::info sizeof mpz_struct]]
    mpz_init_set_str mpz1 $i1 10
    mpz_init_set_str mpz2 $i2 10
    set r [$fn $mpz1 $mpz2]
    mpz_clear mpz1
    mpz_clear mpz2
    set r
}
proc ::gmp::zadd {i1 i2} { zbinary mpz_add $i1 $i2 }
proc ::gmp::zsub {i1 i2} { zbinary mpz_sub $i1 $i2 }
proc ::gmp::zmul {i1 i2} { zbinary mpz_mul $i1 $i2 }
proc ::gmp::zdiv {i1 i2} { zbinary mpz_tdiv_q $i1 $i2 }
proc ::gmp::zrem {i1 i2} { zbinary mpz_tdiv_r $i1 $i2 }
proc ::gmp::zmod {i1 i2} { zbinary mpz_mod $i1 $i2 }
proc ::gmp::zgcd {i1 i2} { zbinary mpz_gcd $i1 $i2 }
proc ::gmp::zcmp {i1 i2} { zbinaryi mpz_cmp $i1 $i2 }
proc ::gmp::zand {i1 i2} { zbinary mpz_and $i1 $i2 }
proc ::gmp::zior {i1 i2} { zbinary mpz_ior $i1 $i2 }
proc ::gmp::zcom {i1} { zunary mpz_com $i1 }
proc ::gmp::zneg {i1} { zunary mpz_neg $i1 }
proc ::gmp::zabs {i1} { zunary mpz_abs $i1 }
proc ::gmp::zsqrt {i1} { zunary mpz_sqrt $i1 }
proc ::gmp::zcvt {i1 rad1 rad2} {
    set mpz1 [binary format x[::ffidl::info sizeof mpz_struct]]
    mpz_init_set_str mpz1 $i1 $rad1
    set r [zget_str $rad2 $mpz1]
    mpz_clear mpz1
    set r
}
