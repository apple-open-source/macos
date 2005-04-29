# pdf_stat.tcl --
#
#    Collection of procedures for evaluating probability and
#    cumulative density functions
#    Part of "math::statistics"
#
# version 0.1: initial implementation, january 2003

# ::math::statistics --
#   Namespace holding the procedures and variables
#
namespace eval ::math::statistics {

    namespace export pdf-normal pdf-uniform \
	    pdf-exponential \
	    cdf-normal cdf-uniform \
	    cdf-exponential \
	    cdf-students-t \
	    random-normal random-uniform \
	    random-exponential \
	    histogram-uniform

    variable cdf_normal_prob     {}
    variable cdf_normal_x        {}
    variable cdf_toms322_cached  {}
}

# pdf-normal --
#    Return the probabilities belonging to a normal distribution
#
# Arguments:
#    mean     Mean of the distribution
#    stdev    Standard deviation
#    x        Value for which the probability must be determined
#
# Result:
#    Probability of value x under the given distribution
#
proc ::math::statistics::pdf-normal { mean stdev x } {
    variable NEGSTDEV
    variable factorNormalPdf

    if { $stdev <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $NEGSTDEV $NEGSTDEV
    }

    set xn   [expr {($x-$mean)/$stdev}]
    set prob [expr {exp(-$xn*$xn/2.0)/$stdev/$factorNormalPdf}]

    return $prob
}

# pdf-uniform --
#    Return the probabilities belonging to a uniform distribution
#    (parameters as minimum/maximum)
#
# Arguments:
#    pmin      Minimum of the distribution
#    pmax      Maximum of the distribution
#    x         Value for which the probability must be determined
#
# Result:
#    Probability of value x under the given distribution
#
proc ::math::statistics::pdf-uniform { pmin pmax x } {

    if { $pmin >= $pmax } {
	return -code error -errorcode ARG \
		-errorinfo "Wrong order or zero range" \
		"Wrong order or zero range"
    }

    set prob [expr {1.0/($pmax-$min)}]

    if { $x < $pmin || $x > $pmax } { return 0.0 }

    return $prob
}

# pdf-exponential --
#    Return the probabilities belonging to an exponential
#    distribution
#
# Arguments:
#    mean     Mean of the distribution
#    x        Value for which the probability must be determined
#
# Result:
#    Probability of value x under the given distribution
#
proc ::math::statistics::pdf-exponential { mean x } {
    variable NEGSTDEV
    variable OUTOFRANGE

    if { $stdev <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $NEGSTDEV $NEGSTDEV
    }
    if { $mean <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $OUTOFRANGE \
		"$OUTOFRANGE: mean must be positive"
    }

    if { $x < 0.0 } { return 0.0 }
    if { $x > 700.0*$mean } { return 0.0 }

    set prob [expr {exp(-$x/$mean)/$mean}]

    return $prob
}

# cdf-normal --
#    Return the cumulative probability belonging to a normal distribution
#
# Arguments:
#    mean     Mean of the distribution
#    stdev    Standard deviation
#    x        Value for which the probability must be determined
#
# Result:
#    Cumulative probability of value x under the given distribution
#
proc ::math::statistics::cdf-normal { mean stdev x } {
    variable NEGSTDEV

    if { $stdev <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $NEGSTDEV $NEGSTDEV
    }

    set xn    [expr {($x-$mean)/$stdev}]
    set prob1 [Cdf-toms322 1 5000 [expr {$xn*$xn}]]
    if { $xn > 0.0 } {
	set prob [expr {0.5+0.5*$prob1}]
    } else {
	set prob [expr {0.5-0.5*$prob1}]
    }

    return $prob
}

# cdf-students-t --
#    Return the cumulative probability belonging to the
#    Student's t distribution
#
# Arguments:
#    degrees  Number of degrees of freedom
#    x        Value for which the probability must be determined
#
# Result:
#    Cumulative probability of value x under the given distribution
#
proc ::math::statistics::cdf-students-t { degrees x } {

    if { $degrees <= 0 } {
	return -code error -errorcode ARG -errorinfo \
		"Number of degrees of freedom must be positive" \
		"Number of degrees of freedom must be positive"
    }

    set prob1 [Cdf-toms322 1 $degrees [expr {$x*$x}]]
    set prob  [expr {0.5+0.5*$prob1}]

    return $prob
}

# cdf-uniform --
#    Return the cumulative probabilities belonging to a uniform
#    distribution (parameters as minimum/maximum)
#
# Arguments:
#    pmin      Minimum of the distribution
#    pmax      Maximum of the distribution
#    x         Value for which the probability must be determined
#
# Result:
#    Cumulative probability of value x under the given distribution
#
proc ::math::statistics::cdf-uniform { pmin pmax x } {

    if { $pmin >= $pmax } {
	return -code error -errorcode ARG \
		-errorinfo "Wrong order or zero range" \
	    }

    set prob [expr {($x-$pmin)/($pmax-$min)}]

    if { $x < $pmin } { return 0.0 }
    if { $x > $pmax } { return 1.0 }

    return $prob
}

# cdf-exponential --
#    Return the cumulative probabilities belonging to an exponential
#    distribution
#
# Arguments:
#    mean     Mean of the distribution
#    x        Value for which the probability must be determined
#
# Result:
#    Cumulative probability of value x under the given distribution
#
proc ::math::statistics::cdf-exponential { mean x } {
    variable NEGSTDEV
    variable OUTOFRANGE

    if { $mean <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $OUTOFRANGE \
		"$OUTOFRANGE: mean must be positive"
    }

    if { $x <  0.0 } { return 0.0 }
    if { $x > 30.0*$mean } { return 1.0 }

    set prob [expr {1.0-exp(-$x/$mean)}]

    return $prob
}

# Inverse-cdf-uniform --
#    Return the argument belonging to the cumulative probability
#    for a uniform distribution (parameters as minimum/maximum)
#
# Arguments:
#    pmin      Minimum of the distribution
#    pmax      Maximum of the distribution
#    prob      Cumulative probability for which the "x" value must be
#              determined
#
# Result:
#    X value that gives the cumulative probability under the
#    given distribution
#
proc ::math::statistics::Inverse-cdf-uniform { pmin pmax prob } {

    if 0 {
	if { $pmin >= $pmax } {
	    return -code error -errorcode ARG \
		    -errorinfo "Wrong order or zero range" \
		    "Wrong order or zero range"
	}
    }

    set x [expr {$pmin+$prob*($pmax-$pmin)}]

    if { $x < $pmin } { return $pmin }
    if { $x > $pmax } { return $pmax }

    return $x
}

# Inverse-cdf-exponential --
#    Return the argument belonging to the cumulative probability
#    for an exponential distribution
#
# Arguments:
#    mean      Mean of the distribution
#    prob      Cumulative probability for which the "x" value must be
#              determined
#
# Result:
#    X value that gives the cumulative probability under the
#    given distribution
#
proc ::math::statistics::Inverse-cdf-exponential { mean prob } {

    if 0 {
	if { $mean <= 0.0 } {
	    return -code error -errorcode ARG \
		    -errorinfo "Mean must be positive" \
		    "Mean must be positive"
	}
    }

    set x [expr {-$mean*log(1.0-$prob)}]

    return $x
}

# Inverse-cdf-normal --
#    Return the argument belonging to the cumulative probability
#    for a normal distribution
#
# Arguments:
#    mean      Mean of the distribution
#    stdev     Standard deviation of the distribution
#    prob      Cumulative probability for which the "x" value must be
#              determined
#
# Result:
#    X value that gives the cumulative probability under the
#    given distribution
#
proc ::math::statistics::Inverse-cdf-normal { mean stdev prob } {
    variable cdf_normal_prob
    variable cdf_normal_x

    # Look for the proper probability level first,
    # then interpolate
    #
    # Note: the numerical data are connected to the length of
    #       the lists - see Initialise-cdf-normal
    #
    set size 32
    set idx  64
    for { set i 0 } { $i <= 7 } { incr i } {
	set upper [lindex $cdf_normal_prob $idx]
	if { $prob > $upper } {
	    set idx  [expr {$idx+$size}]
	} else {
	    set idx  [expr {$idx-$size}]
	}
	set size [expr {$size/2}]
    }
    #
    # We have found a value that is close to the one we need,
    # now find the enclosing interval
    #
    if { $upper < $prob } {
	incr idx
    }
    set p1 [lindex $cdf_normal_prob [expr {$idx-1}]]
    set p2 [lindex $cdf_normal_prob $idx]
    set x1 [lindex $cdf_normal_x    [expr {$idx-1}]]
    set x2 [lindex $cdf_normal_x    $idx           ]

    set x  [expr {$x1+($x2-$x1)*($prob-$p1)/($p2-$p1)}]

    return [expr {$mean+$stdev*$x}]
}

# Initialise-cdf-normal --
#    Initialise the private data for the normal cdf
#
# Arguments:
#    None
# Result:
#    None
# Side effect:
#    Variable cdf_normal_prob and cdf_normal_x are filled
#    so that we can use these as a look-up table
#
proc ::math::statistics::Initialise-cdf-normal { } {
    variable cdf_normal_prob
    variable cdf_normal_x

    set dx [expr {10.0/128.0}]

    set cdf_normal_prob 0.5
    set cdf_normal_x    0.0
    for { set i 1 } { $i <= 64 } { incr i } {
	set x    [expr {$i*$dx}]
	if { $x != 0.0 } {
	    set prob [Cdf-toms322 1 5000 [expr {$x*$x}]]
	} else {
	    set prob 0.0
	}

	set cdf_normal_x    [concat [expr {-$x}] $cdf_normal_x $x]
	set cdf_normal_prob \
		[concat [expr {0.5-0.5*$prob}] $cdf_normal_prob \
		[expr {0.5+0.5*$prob}]]
    }
}

# random-uniform --
#    Return a list of random numbers satisfying a uniform
#    distribution (parameters as minimum/maximum)
#
# Arguments:
#    pmin      Minimum of the distribution
#    pmax      Maximum of the distribution
#    number    Number of values to generate
#
# Result:
#    List of random numbers
#
proc ::math::statistics::random-uniform { pmin pmax number } {

    if { $pmin >= $pmax } {
	return -code error -errorcode ARG \
		-errorinfo "Wrong order or zero range" \
		"Wrong order or zero range"
    }

    set result {}
    for { set i 0 }  {$i < $number } { incr i } {
	lappend result [Inverse-cdf-uniform $pmin $pmax [expr {rand()}]]
    }

    return $result
}

# random-exponential --
#    Return a list of random numbers satisfying an exponential
#    distribution
#
# Arguments:
#    mean      Mean of the distribution
#    number    Number of values to generate
#
# Result:
#    List of random numbers
#
proc ::math::statistics::random-exponential { mean number } {

    if { $mean <= 0.0 } {
	return -code error -errorcode ARG \
		-errorinfo "Mean must be positive" \
		"Mean must be positive"
    }

    set result {}
    for { set i 0 }  {$i < $number } { incr i } {
	lappend result [Inverse-cdf-exponential $mean [expr {rand()}]]
    }

    return $result
}

# random-normal --
#    Return a list of random numbers satisfying a normal
#    distribution
#
# Arguments:
#    mean      Mean of the distribution
#    stdev     Standard deviation of the distribution
#    number    Number of values to generate
#
# Result:
#    List of random numbers
#
proc ::math::statistics::random-normal { mean stdev number } {

    if { $stdev <= 0.0 } {
	return -code error -errorcode ARG \
		-errorinfo "Standard deviation must be positive" \
		"Standard deviation must be positive"
    }

    set result {}
    for { set i 0 }  {$i < $number } { incr i } {
	lappend result [Inverse-cdf-normal $mean $stdev [expr {rand()}]]
    }

    return $result
}

# Cdf-toms322 --
#    Calculate the cumulative density function for several distributions
#    according to TOMS322
#
# Arguments:
#    m         First number of degrees of freedom
#    n         Second number of degrees of freedom
#    x         Value for which the cdf must be calculated
#
# Result:
#    Cumulatve density at x - details depend on distribution
#
# Notes:
#    F-ratios:
#        m - degrees of freedom for numerator
#        n - degrees of freedom for denominator
#        x - F-ratio
#    Student's t (two-tailed):
#        m - 1
#        n - degrees of freedom
#        x - square of t
#    Normal deviate (two-tailed):
#        m - 1
#        n - 5000
#        x - square of deviate
#    Chi-square:
#        m - degrees of freedom
#        n - 5000
#        x - chi-square/m
#    The original code can be found at <http://www.netlib.org>
#
proc ::math::statistics::Cdf-toms322 { m n x } {
    set m [expr {$m < 300?  int($m) : 300}]
    set n [expr {$n < 5000? int($n) : 5000}]
    if { $m < 1 || $n < 1 } {
	return -code error -errorcode ARG \
		-errorinfo "Arguments m anf n must be greater/equal 1"
	"Arguments m anf n must be greater/equal 1"
    }

    set a [expr {2*($m/2)-$m+2}]
    set b [expr {2*($n/2)-$n+2}]
    set w [expr {$x*double($m)/double($n)}]
    set z [expr {1.0/(1.0+$w)}]

    if { $a == 1 } {
	if { $b == 1 } {
	    set p [expr {sqrt($w)}]
	    set y 0.3183098862
	    set d [expr {$y*$z/$p}]
	    set p [expr {2.0*$y*atan($p)}]
	} else {
	    set p [expr {sqrt($w*$z)}]
	    set d [expr {$p*$z/(2.0*$w)}]
	}
    } else {
	if { $b == 1 } {
	    set p [expr {sqrt($z)}]
	    set d [expr {$z*$p/2.0}]
	    set p [expr {1.0-$p}]
	} else {
	    set d [expr {$z*$z}]
	    set p [expr {$z*$w}]
	}
    }

    set y [expr {2.0*$w/$z}]

    if { $a == 1 } {
	for { set j [expr {$b+2}] } { $j <= $n } { incr j 2 } {
	    set d [expr {(1.0+double($a)/double($j-2)) * $d*$z}]
	    set p [expr {$p+$d*$y/double($j-1)}]
	}
    } else {
	set power [expr {($n-1)/2}]
	set zk    [expr {pow($z,$power)}]
	set d     [expr {($d*$zk*$n)/$b}]
	set p     [expr {$p*$zk + $w*$z * ($zk-1.0)/($z-1.0)}]
    }

    set y [expr {$w*$z}]
    set z [expr {2.0/$z}]
    set b [expr {$n-2}]

    for { set i [expr {$a+2}] } { $i <= $m } { incr i 2 } {
	set j [expr {$i+$b}]
	set d [expr {$y*$d*double($j)/double($i-2)}]
	set p [expr {$p-$z*$d/double($j)}]
    }
    set prob $p
    if  { $prob < 0.0 } { set prob 0.0 }
    if  { $prob > 1.0 } { set prob 1.0 }

    return $prob
}

# Inverse-cdf-toms322 --
#    Return the argument belonging to the cumulative probability
#    for an F, chi-square or t distribution
#
# Arguments:
#    m         First number of degrees of freedom
#    n         Second number of degrees of freedom
#    prob      Cumulative probability for which the "x" value must be
#              determined
#
# Result:
#    X value that gives the cumulative probability under the
#    given distribution
#
# Note:
#    See the procedure Cdf-toms322 for more details
#
proc ::math::statistics::Inverse-cdf-toms322 { m n prob } {
    variable cdf_toms322_cached
    variable OUTOFRANGE

    if { $prob <= 0 || $prob >= 1 } {
	return -code error -errorcode $OUTOFRANGE $OUTOFRANGE
    }

    # Is the combination in cache? Then we can simply rely
    # on that
    #
    foreach {m1 n1 prob1 x1} $cdf_toms322_cached {
	if { $m1 == $m && $n1 == $n && $prob1 == $prob } {
	    return $x1
	}
    }

    #
    # Otherwise first find a value of x for which Cdf(x) exceeds prob
    #
    set x1  1.0
    set dx1 1.0
    while { [Cdf-toms322 $m $n $x1] < $prob } {
	set x1  [expr {$x1+$dx1}]
	set dx1 [expr {2.0*$dx1}]
    }

    #
    # Now, look closer
    #
    while { $dx1 > 0.0001 } {
	set p1 [Cdf-toms322 $m $n $x1]
	if { $p1 > $prob } {
	    set x1  [expr {$x1-$dx1}]
	} else {
	    set x1  [expr {$x1+$dx1}]
	}
	set dx1 [expr {$dx1/2.0}]
    }

    #
    # Cache the result
    #
    set last end
    if { [llength $cdf_toms322_cached] > 27 } {
	set last 26
    }
    set cdf_toms322_cached \
	    [concat [list $m $n $prob $x1] [lrange $cdf_toms322_cached 0 $last]]

    return $x1
}

# HistogramMake --
#    Distribute the "observations" according to the cdf
#
# Arguments:
#    cdf-values   Values for the cdf (relative number of observations)
#    number       Total number of "observations" in the histogram
#
# Result:
#    List of numbers, distributed over the buckets
#
proc ::math::statistics::HistogramMake { cdf-values number } {

    set assigned  0
    set result    {}
    set residue   0.0
    foreach cdfv $cdf-values {
	set sum      [expr {$number*($cdfv + $residue)}]
	set bucket   [expr {int($sum)}]
	set residue  [expr {$sum-$bucket}]
	set assigned [expr {$assigned-$bucket}]
	lappend result $bucket
    }
    set remaining [expr {$number-$assigned}]
    if { $remaining > 0 } {
	lappend result $remaining
    } else {
	lappend result 0
    }

    return $result
}

# histogram-uniform --
#    Return the expected histogram for a uniform distribution
#
# Arguments:
#    min       Minimum the distribution
#    max       Maximum the distribution
#    limits    upper limits for the histogram buckets
#    number    Total number of "observations" in the histogram
#
# Result:
#    List of expected number of observations
#
proc ::math::statistics::histogram-uniform { min max limits number } {
    if { $min >= $max } {
	return -code error -errorcode ARG \
		-errorinfo "Wrong order or zero range" \
		"Wrong order or zero range"
    }

    set cdf_result {}
    foreach limit $limits {
	lappend cdf_result [cdf-uniform $min $max $limit]
    }

    return [HistogramMake $cdf_result $number]
}

# Initialisation --
#    Initialise some parameters
#
namespace eval ::math::statistics {
    Initialise-cdf-normal
}

#
# Simple numerical tests
#
if { [file tail [info script]] == [file tail $::argv0] } {

    #
    # Apparent accuracy: at least one digit more than the ones in the
    # given numbers
    #
    puts "Normal distribution - two-tailed"
    foreach z    {4.417 3.891 3.291 2.576 2.241 1.960 1.645 1.150 0.674
    0.319 0.126 0.063 0.0125} \
	    pexp {1.e-5 1.e-4 1.e-3 1.e-2 0.025 0.050 0.100 0.250 0.500
    0.750 0.900 0.950 0.990 } {
	set prob [::math::statistics::Cdf-toms322 1 5000 [expr {$z*$z}]]
	puts "$z - $pexp - [expr {1.0-$prob}]"
    }

    puts "Normal distribution (inverted; one-tailed)"
    foreach p {0.001 0.01 0.1 0.25 0.5 0.75 0.9 0.99 0.999} {
	puts "$p - [::math::statistics::Inverse-cdf-normal 0.0 1.0 $p]"
    }
    puts "Normal random variables"
    set rndvars [::math::statistics::random-normal 1.0 2.0 20]
    puts $rndvars
    puts "Normal uniform variables"
    set rndvars [::math::statistics::random-uniform 1.0 2.0 20]
    puts $rndvars
    puts "Normal exponential variables"
    set rndvars [::math::statistics::random-exponential 2.0 20]
    puts $rndvars
}
