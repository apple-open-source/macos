#
# a binding to libm using ffidl
#

package provide Libm 0.1
package require Ffidl
package require Ffidlrt

namespace eval ::libm:: {
    #
    # the name of libm
    #
    set lib [::ffidl::find-lib m]

    #
    # a type map
    #
    array set types {
	void void
	int int
	double double
	{OUT int *} pointer-var
	{OUT double *} pointer-var
    }
    #
    # create bindings from prototypes
    #
    foreach proto {
	{double acos(double)}
	{double asin(double)}
	{double atan(double)}
	{double atan2(double, double)}
	{double cos(double)}
	{double sin(double)}
	{double tan(double)}
	{double cosh(double)}
	{double sinh(double)}
	{double tanh(double)}
	{double acosh(double)}
	{double asinh(double)}
	{double atanh(double)}
	{double exp(double)}
	{double frexp(double, OUT int *)}
	{double ldexp(double, int)}
	{double log(double)}
	{double log10(double)}
	{double expm1(double)}
	{double log1p(double)}
	{double logb(double)}
	{double modf(double, OUT double *)}
	{double pow(double, double)}
	{double sqrt(double)}
	{double cbrt(double)}
	{double ceil(double)}
	{double fabs(double)}
	{double floor(double)}
	{double fmod(double, double)}
	{int isinf(double)}
	{int finite(double)}
	{double copysign(double, double)}
	{double scalbn(double, int)}
	{double drem(double, double)}
	{double significand(double)}
	{int isnan(double)}
	{int ilogb(double)}
	{double hypot(double, double)}
	{double erf(double)}
	{double erfc(double)}
	{double gamma(double)}
	{double j0(double)}
	{double j1(double)}
	{double jn(int, double)}
	{double lgamma(double)}
	{double y0(double)}
	{double y1(double)}
	{double yn(int, double)}
	{double gamma_r(double, OUT int *)}
	{double lgamma_r(double, OUT int *)}
	{double rint(double)}
	{double scalb(double, double)}
	{double nextafter(double, double)}
	{double remainder(double, double)}
    } {
	if { ! [regexp {^([a-z_]+) ([a-z0-9_]+)\((.*)\)$} $proto all rtype name args]} {
	    puts "malformed function declaration: $proto"
	    continue
	}
	if {[catch {::ffidl::symbol $lib $name} addr]} {
	    #puts "function is not defined in \"$lib\": \"$name\"\n$addr"
	    continue
	}
	set argout {}
	foreach atype [split $args ,] {
	    set atype [string trim $atype]
	    lappend argout $types($atype)
	}
	set rtype [string trim $rtype]
	set retout $types($rtype)
	::ffidl::callout ::libm::$name $argout $retout $addr
	namespace export $name
    }
}

