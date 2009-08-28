package provide Gmpf 2.0
package require Gmpq 2.0

#
# interface to arbitrary precison floating point arithmetic
# 
namespace eval ::gmp:: {

    namespace export fadd fsub fmul fdiv fcmp feq fneg fabs fsqrt fcvt

    mpf_set_default_prec 256
}

proc ::gmp::fget_str {rad mpf} {
    set p [mpf_get_prec $mpf]
    if {$rad < 2 || $rad > 36} {
	error "radix $rad out of bounds, min 2, max 36"
    } elseif {$rad == 2} {
	set nd $p
    } elseif {$rad <= 4} {
	set nd [expr {$p/2}]
    } elseif {$rad <= 8} {
	set nd [expr {$p/3}]
    } elseif {$rad <= 16} {
	set nd [expr {$p/4}]
    } elseif {$rad <= 32} {
	set nd [expr {$p/5}]
    } else {
	set nd [expr {$p/6}]
    }
    set mpfdig [binary format x[expr {$nd+2}]]
    set mpfexp [binary format x[::ffidl::info sizeof mp_exp_t]]
    set digits [mpf_get_str mpfdig mpfexp $rad $nd $mpf]
    binary scan $mpfexp [::ffidl::info format mp_exp_t] exp
    if {[string compare [string index $digits 0] -] == 0} {
	set digits -0.[string range $digits 1 end]
    } else {
	set digits 0.$digits
    }
    if {$rad == 10} {
	return "${digits}e$exp"
    } else {
	return "${digits}@$exp"
    }
}
proc ::gmp::funary {fn f1} {
    set mpf1 [binary format x[::ffidl::info sizeof mpf_struct]]
    set mpf2 [binary format x[::ffidl::info sizeof mpf_struct]]
    mpf_init_set_str mpf1 $f1 10
    mpf_init mpf2
    $fn mpf2 $mpf1
    set r [fget_str 10 $mpf2]
    mpf_clear mpf1
    mpf_clear mpf2
    set r
}
proc ::gmp::fbinary {fn f1 f2} {
    set mpf1 [binary format x[::ffidl::info sizeof mpf_struct]]
    set mpf2 [binary format x[::ffidl::info sizeof mpf_struct]]
    set mpf3 [binary format x[::ffidl::info sizeof mpf_struct]]
    mpf_init_set_str mpf1 $f1 10
    mpf_init_set_str mpf2 $f2 10
    mpf_init mpf3
    $fn mpf3 $mpf1 $mpf2
    set r [fget_str 10 $mpf3]
    mpf_clear mpf1
    mpf_clear mpf2
    mpf_clear mpf3
    set r
}
proc ::gmp::fbinaryi {fn f1 f2} {
    set mpf1 [binary format x[::ffidl::info sizeof mpf_struct]]
    set mpf2 [binary format x[::ffidl::info sizeof mpf_struct]]
    mpf_init_set_str mpf1 $f1 10
    mpf_init_set_str mpf2 $f2 10
    set r [$fn $mpf1 $mpf2]
    mpf_clear mpf1
    mpf_clear mpf2
    set r
}
proc ::gmp::fsetprecision {nbits} { mpf_set_default_prec $nbits }
proc ::gmp::fadd {f1 f2} { fbinary mpf_add $f1 $f2 }
proc ::gmp::fsub {f1 f2} { fbinary mpf_sub $f1 $f2 }
proc ::gmp::fmul {f1 f2} { fbinary mpf_mul $f1 $f2 }
proc ::gmp::fdiv {f1 f2} { fbinary mpf_div $f1 $f2}
proc ::gmp::fcmp {f1 f2} { fbinaryi mpf_cmp $f1 $f2}
proc ::gmp::fneg {f1} { funary mpf_neg $f1 }
proc ::gmp::fabs {f1} { funary mpf_abs $f1 }
proc ::gmp::fsqrt {f1} { funary mpf_sqrt $f1 }
proc ::gmp::fcvt {f1 rad1 rad2} {
    set mpf1 [binary format x[::ffidl::info sizeof mpf_struct]]
    mpf_init_set_str mpf1 $f1 $rad1
    set r [zget_str $rad2 $mpf1]
    mpf_clear mpf1
    set r
}
