#
# Ffidl interface to GNU multiple precision arithmetic library
#
# gmp-2.0.1 doesn't offer a shared library as a configuration
# option, but it wasn't too hard to do.  Harder are the functions
# implemented only as macros.
#

package provide Gmp 2.0
package require Ffidl 0.1
package require Ffidlrt 0.1

#
# typedefs
#
::ffidl::find-type size_t
::ffidl::typedef mp_exp_t long
::ffidl::typedef mp_size_t long
::ffidl::typedef mp_limb_t {unsigned long}
::ffidl::typedef mp_limb_signed_t long
::ffidl::typedef mpz_struct int int pointer
::ffidl::typedef mpq_struct mpz_struct mpz_struct
::ffidl::typedef mpf_struct int int mp_exp_t pointer

namespace eval ::gmp:: {

    set lib [::ffidl::find-lib gmp]

    array set types {

	mpz_ptr pointer-var
	mpz_srcptr pointer-byte

	mpq_ptr pointer-var
	mpq_srcptr pointer-byte

	mpf_ptr pointer-var
	mpf_srcptr pointer-byte

	mp_size_t mp_size_t
	mp_limb_t mp_limb_t
	mp_limb_signed_t mp_limb_signed_t
	{mp_exp_t *} {pointer-var}
	mp_exp_t mp_exp_t

	size_t size_t
	{unsigned long int} {unsigned long}
	{signed long int} long
	{long int} long
	{const char *} {pointer-utf8}
	{char *} pointer-var
	double double
	void void
	int int
    }

    #
    # prototypes for integer, rational, and real functions
    #
    foreach proto {
	{void mpz_abs (mpz_ptr, mpz_srcptr)}
	{void mpz_add (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_add_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_and (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_array_init (mpz_ptr, mp_size_t, mp_size_t)}
	{void mpz_cdiv_q (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{unsigned long int mpz_cdiv_q_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_cdiv_qr (mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{unsigned long int mpz_cdiv_qr_ui (mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_cdiv_r (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{unsigned long int mpz_cdiv_r_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{unsigned long int mpz_cdiv_ui (mpz_srcptr, unsigned long int)}
	{void mpz_clear (mpz_ptr)}
	{void mpz_clrbit (mpz_ptr, unsigned long int)}
	{int mpz_cmp (mpz_srcptr, mpz_srcptr)}
	{int mpz_cmp_si (mpz_srcptr, signed long int)}
	{int mpz_cmp_ui (mpz_srcptr, unsigned long int)}
	{void mpz_com (mpz_ptr, mpz_srcptr)}
	{void mpz_divexact (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_fac_ui (mpz_ptr, unsigned long int)}
	{void mpz_fdiv_q (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_fdiv_q_2exp (mpz_ptr, mpz_srcptr, unsigned long int)}
	{unsigned long int mpz_fdiv_q_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_fdiv_qr (mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{unsigned long int mpz_fdiv_qr_ui (mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_fdiv_r (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_fdiv_r_2exp (mpz_ptr, mpz_srcptr, unsigned long int)}
	{unsigned long int mpz_fdiv_r_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{unsigned long int mpz_fdiv_ui (mpz_srcptr, unsigned long int)}
	{void mpz_gcd (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{unsigned long int mpz_gcd_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_gcdext (mpz_ptr, mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{double mpz_get_d (mpz_srcptr)}
	{long int mpz_get_si (mpz_srcptr)}
	{char *mpz_get_str (char *, int, mpz_srcptr)}
	{unsigned long int mpz_get_ui (mpz_srcptr)}
	{mp_limb_t mpz_getlimbn (mpz_srcptr, mp_size_t)}
	{unsigned long int mpz_hamdist (mpz_srcptr, mpz_srcptr)}
	{void mpz_init (mpz_ptr)}
	{void mpz_init_set (mpz_ptr, mpz_srcptr)}
	{void mpz_init_set_d (mpz_ptr, double)}
	{void mpz_init_set_si (mpz_ptr, signed long int)}
	{int mpz_init_set_str (mpz_ptr, const char *, int)}
	{void mpz_init_set_ui (mpz_ptr, unsigned long int)}
	{int mpz_invert (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_ior (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{int mpz_jacobi (mpz_srcptr, mpz_srcptr)}
	{int mpz_legendre (mpz_srcptr, mpz_srcptr)}
	{void mpz_mod (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_mul (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_mul_2exp (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_mul_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_neg (mpz_ptr, mpz_srcptr)}
	{int mpz_perfect_square_p (mpz_srcptr)}
	{unsigned long int mpz_popcount (mpz_srcptr)}
	{void mpz_pow_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_powm (mpz_ptr, mpz_srcptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_powm_ui (mpz_ptr, mpz_srcptr, unsigned long int, mpz_srcptr)}
	{int mpz_probab_prime_p (mpz_srcptr, int)}
	{void mpz_random (mpz_ptr, mp_size_t)}
	{void mpz_random2 (mpz_ptr, mp_size_t)}
	{unsigned long int mpz_scan0 (mpz_srcptr, unsigned long int)}
	{unsigned long int mpz_scan1 (mpz_srcptr, unsigned long int)}
	{void mpz_set (mpz_ptr, mpz_srcptr)}
	{void mpz_set_d (mpz_ptr, double)}
	{void mpz_set_f (mpz_ptr, mpf_srcptr)}
	{void mpz_set_q (mpz_ptr, mpq_srcptr)}
	{void mpz_set_si (mpz_ptr, signed long int)}
	{int mpz_set_str (mpz_ptr, const char *, int)}
	{void mpz_set_ui (mpz_ptr, unsigned long int)}
	{void mpz_setbit (mpz_ptr, unsigned long int)}
	{size_t mpz_size (mpz_srcptr)}
	{size_t mpz_sizeinbase (mpz_srcptr, int)}
	{void mpz_sqrt (mpz_ptr, mpz_srcptr)}
	{void mpz_sqrtrem (mpz_ptr, mpz_ptr, mpz_srcptr)}
	{void mpz_sub (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_sub_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_tdiv_q (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_tdiv_q_2exp (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_tdiv_q_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_tdiv_qr (mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_tdiv_qr_ui (mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_tdiv_r (mpz_ptr, mpz_srcptr, mpz_srcptr)}
	{void mpz_tdiv_r_2exp (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_tdiv_r_ui (mpz_ptr, mpz_srcptr, unsigned long int)}
	{void mpz_ui_pow_ui (mpz_ptr, unsigned long int, unsigned long int)}

	{void mpq_init (mpq_ptr)}
	{void mpq_clear (mpq_ptr)}
	{void mpq_set (mpq_ptr, mpq_srcptr)}
	{void mpq_set_ui (mpq_ptr, unsigned long int, unsigned long int)}
	{void mpq_set_si (mpq_ptr, signed long int, unsigned long int)}
	{void mpq_set_z (mpq_ptr, mpz_srcptr)}
	{void mpq_add (mpq_ptr, mpq_srcptr, mpq_srcptr)}
	{void mpq_sub (mpq_ptr, mpq_srcptr, mpq_srcptr)}
	{void mpq_mul (mpq_ptr, mpq_srcptr, mpq_srcptr)}
	{void mpq_div (mpq_ptr, mpq_srcptr, mpq_srcptr)}
	{void mpq_neg (mpq_ptr, mpq_srcptr)}
	{int mpq_cmp (mpq_srcptr, mpq_srcptr)}
	{int mpq_cmp_ui (mpq_srcptr, unsigned long int, unsigned long int)}
	{int mpq_equal (mpq_srcptr, mpq_srcptr)}
	{void mpq_inv (mpq_ptr, mpq_srcptr)}
	{void mpq_set_num (mpq_ptr, mpz_srcptr)}
	{void mpq_set_den (mpq_ptr, mpz_srcptr)}
	{void mpq_get_num (mpz_ptr, mpq_srcptr)}
	{void mpq_get_den (mpz_ptr, mpq_srcptr)}
	{double mpq_get_d (mpq_srcptr)}
	{void mpq_canonicalize (mpq_ptr)}

	{void mpf_abs (mpf_ptr, mpf_srcptr)}
	{void mpf_add (mpf_ptr, mpf_srcptr, mpf_srcptr)}
	{void mpf_add_ui (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_clear (mpf_ptr)}
	{int mpf_cmp (mpf_srcptr, mpf_srcptr)}
	{int mpf_cmp_si (mpf_srcptr, signed long int)}
	{int mpf_cmp_ui (mpf_srcptr, unsigned long int)}
	{void mpf_div (mpf_ptr, mpf_srcptr, mpf_srcptr)}
	{void mpf_div_2exp (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_div_ui (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_dump (mpf_srcptr)}
	{int mpf_eq (mpf_srcptr, mpf_srcptr, unsigned long int)}
	{double mpf_get_d (mpf_srcptr)}
	{unsigned long int mpf_get_prec (mpf_srcptr)}
	{char *mpf_get_str (char *, mp_exp_t *, int, size_t, mpf_srcptr)}
	{void mpf_init (mpf_ptr)}
	{void mpf_init2 (mpf_ptr, unsigned long int)}
	{void mpf_init_set (mpf_ptr, mpf_srcptr)}
	{void mpf_init_set_d (mpf_ptr, double)}
	{void mpf_init_set_si (mpf_ptr, signed long int)}
	{int mpf_init_set_str (mpf_ptr, const char *, int)}
	{void mpf_init_set_ui (mpf_ptr, unsigned long int)}
	{void mpf_mul (mpf_ptr, mpf_srcptr, mpf_srcptr)}
	{void mpf_mul_2exp (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_mul_ui (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_neg (mpf_ptr, mpf_srcptr)}
	{void mpf_random2 (mpf_ptr, mp_size_t, mp_exp_t)}
	{void mpf_reldiff (mpf_ptr, mpf_srcptr, mpf_srcptr)}
	{void mpf_set (mpf_ptr, mpf_srcptr)}
	{void mpf_set_d (mpf_ptr, double)}
	{void mpf_set_default_prec (unsigned long int)}
	{void mpf_set_prec (mpf_ptr, unsigned long int)}
	{void mpf_set_prec_raw (mpf_ptr, unsigned long int)}
	{void mpf_set_q (mpf_ptr, mpq_srcptr)}
	{void mpf_set_si (mpf_ptr, signed long int)}
	{int mpf_set_str (mpf_ptr, const char *, int)}
	{void mpf_set_ui (mpf_ptr, unsigned long int)}
	{void mpf_set_z (mpf_ptr, mpz_srcptr)}
	{size_t mpf_size (mpf_srcptr)}
	{void mpf_sqrt (mpf_ptr, mpf_srcptr)}
	{void mpf_sqrt_ui (mpf_ptr, unsigned long int)}
	{void mpf_sub (mpf_ptr, mpf_srcptr, mpf_srcptr)}
	{void mpf_sub_ui (mpf_ptr, mpf_srcptr, unsigned long int)}
	{void mpf_ui_div (mpf_ptr, unsigned long int, mpf_srcptr)}
	{void mpf_ui_sub (mpf_ptr, unsigned long int, mpf_srcptr)}

    } {
	if {[regexp {^([a-z_ ]+)( | \*)([a-z_][a-z0-9_]+) \(([a-z_ *,]+)\)$} $proto all \
		 rtype rstar fname atypes]} {
	    set rtype "$rtype$rstar"
	    switch -exact [string trim $rtype] {
		{char *} { set rtypeo pointer-utf8 }
		default { set rtypeo $types([string trim $rtype]) }
	    }
	    set atypeso {}
	    foreach atype [split $atypes ,] {
		lappend atypeso $types([string trim $atype])
	    }
	    ::ffidl::callout ::gmp::$fname $atypeso $rtypeo [::ffidl::symbol $lib $fname]
	    namespace export $fname
	} else {
	    puts "regexp failed on: $proto"
	}
    }
    
}

