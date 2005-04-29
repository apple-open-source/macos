# statistics.tcl --
#
#    Package for basic statistical analysis
#
# version 0.1: initial implementation, january 2003

package provide math::statistics 0.1.1

# ::math::statistics --
#   Namespace holding the procedures and variables
#

namespace eval ::math::statistics {
    #
    # Safer: change to short procedures
    #
    namespace export mean min max number var stdev basic-stats corr \
	    histogram interval-mean-stdev test-mean quantiles \
	    autocorr crosscorr filter map samplescount
    #
    # Error messages
    #
    variable NEGSTDEV   {Zero or negative standard deviation}
    variable TOOFEWDATA {Too few or invalid data}
    variable OUTOFRANGE {Argument out of range}

    #
    # Coefficients involved
    #
    variable factorNormalPdf
    set factorNormalPdf [expr {sqrt(8.0*atan(1.0))}]
}

# mean, min, max, number, var, stdev --
#    Return the mean (minimum, maximum) value of a list of numbers
#    or number of non-missing values
#
# Arguments:
#    type     Type of value to be returned
#    values   List of values to be examined
#
# Results:
#    Value that was required
#
#
namespace eval ::math::statistics {
    foreach type {mean min max number stdev var} {
	proc $type { values } "BasicStats $type \$values"
    }
    proc basic-stats { values } "BasicStats all \$values"
}

# BasicStats --
#    Return the one or all of the basic statistical properties
#
# Arguments:
#    type     Type of value to be returned
#    values   List of values to be examined
#
# Results:
#    Value that was required
#
proc ::math::statistics::BasicStats { type values } {
    variable TOOFEWDATA

    set min    {}
    set max    {}
    set mean   {}
    set stdev  {}
    set var    {}

    set sum    0.0
    set sumsq  0.0
    set number 0

    foreach value $values {
	if { $value == {} } {
	    continue
	}
	set value [expr {double($value)}]

	incr number
	set  sum    [expr {$sum+$value}]
	set  sumsq  [expr {$sumsq+$value*$value}]

	if { $min == {} || $value < $min } {
	    set min $value
	}
	if { $max == {} || $value > $max } {
	    set max $value
	}
    }

    if { $number > 0 } {
	set mean [expr {$sum/$number}]
    } else {
	return -code error -errorcode DATA -errorinfo $TOOFEWDATA $TOOFEWDATA
    }

    if { $number > 1 } {
	set var    [expr {($sumsq-$mean*$sum)/double($number-1)}]
        #
        # Take care of a rare situation: uniform data might
        # cause a tiny negative difference
        #
        if { $var < 0.0 } {
           set var 0.0
        }
	set stdev  [expr {sqrt($var)}]
    }

    set all [list $mean $min $max $number $stdev $var]

    #
    # Return the appropriate value
    #
    if { [lsearch {all mean min max number stdev var} $type] >= 0 } {
	# FRINK: nocheck
	return [set $type]
    } else {
	return -code error \
		-errorcode ARG -errorinfo [list unknown type of statistic -- $type] \
		[list unknown type of statistic -- $type]
    }
}

# histogram --
#    Return histogram information from a list of numbers
#
# Arguments:
#    limits   Upper limits for the buckets (in increasing order)
#    values   List of values to be examined
#
# Results:
#    List of number of values in each bucket (length is one more than
#    the number of limits)
#
#
proc ::math::statistics::histogram { limits values } {

    if { [llength $limits] < 1 } {
	return -code error -errorcode ARG -errorinfo {No limits given} {No limits given}
    }

    set limits [lsort -real -increasing $limits]

    for { set index 0 } { $index <= [llength $limits] } { incr index } {
	set buckets($index) 0
    }
    set last [llength $limits]

    foreach value $values {
	if { $value == {} } {
	    continue
	}

	set index 0
	set found 0
	foreach limit $limits {
	    if { $value <= $limit } {
		set found 1
		incr buckets($index)
		break
	    }
	    incr index
	}

	if { $found == 0 } {
	    incr buckets($last)
	}
    }

    set result {}
    for { set index 0 } { $index <= $last } { incr index } {
	lappend result $buckets($index)
    }

    return $result
}

# corr --
#    Return the correlation coefficient of two sets of data
#
# Arguments:
#    data1    List with the first set of data
#    data2    List with the second set of data
#
# Result:
#    Correlation coefficient of the two
#
proc ::math::statistics::corr { data1 data2 } {
    variable TOOFEWDATA

    set number  0
    set sum1    0.0
    set sum2    0.0
    set sumsq1  0.0
    set sumsq2  0.0
    set sumprod 0.0

    foreach value1 $data1 value2 $data2 {
	if { $value1 == {} || $value2 == {} } {
	    continue
	}
	set  value1  [expr {double($value1)}]
	set  value2  [expr {double($value2)}]

	set  sum1    [expr {$sum1+$value1}]
	set  sum2    [expr {$sum2+$value2}]
	set  sumsq1  [expr {$sumsq1+$value1*$value1}]
	set  sumsq2  [expr {$sumsq2+$value2*$value2}]
	set  sumprod [expr {$sumprod+$value1*$value2}]
	incr number
    }
    if { $number > 0 } {
	set numerator   [expr {$number*$sumprod-$sum1*$sum2}]
	set denom1      [expr {sqrt($number*$sumsq1-$sum1*$sum1)}]
	set denom2      [expr {sqrt($number*$sumsq2-$sum2*$sum2)}]
	if { $denom1 != 0.0 && $denom2 != 0.0 } {
	    set corr_coeff  [expr {$numerator/$denom1/$denom2}]
	} elseif { $denom1 != 0.0 || $denom2 != 0.0 } {
	    set corr_coeff  0.0 ;# Uniform against non-uniform
	} else {
	    set corr_coeff  1.0 ;# Both uniform
	}

    } else {
	return -code error -errorcode DATA -errorinfo $TOOFEWDATA $TOOFEWDATA
    }
    return $corr_coeff
}

# test-normal --
#    Test whether the data are distributed according to the normal
#    distribution with a certain level of confidence
#
# Arguments:
#    data         List of raw data values
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    1 if the test shows a normal distribution, 0 otherwise
#
proc ::math::statistics::test-normal { data confidence } {
    ...
}

# t-test-mean --
#    Test whether the mean value of a sample is in accordance with the
#    estimated normal distribution with a certain level of confidence
#    (Student's t test)
#
# Arguments:
#    data         List of raw data values (small sample)
#    est_mean     Estimated mean of the distribution
#    est_stdev    Estimated stdev of the distribution
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    1 if the test is positive, 0 otherwise. If there are too few data,
#    returns an empty string
#
proc ::math::statistics::t-test-mean { data est_mean est_stdev confidence } {
    variable NEGSTDEV
    variable TOOFEWDATA

    if { $est_stdev <= 0.0 } {
	return -code error -errorcode ARG -errorinfo $NEGSTDEV $NEGSTDEV
    }

    set allstats        [BasicStats all $data]

    set conf2           [expr {(1.0+$confidence)/2.0}]

    set sample_mean     [lindex $allstats 0]
    set sample_number   [lindex $allstats 3]

    if { $sample_number > 1 } {
	set tzero   [expr {abs($sample_mean-$est_mean)/$est_stdev * \
		sqrt($sample_number-1)}]
	set degrees [expr {$sample_number-1}]
	set prob    [cdf-students-t $degrees $tzero]

	return [expr {$prob<$conf2}]

    } else {
	return -code error -errorcode DATA -errorinfo $TOOFEWDATA $TOOFEWDATA
    }
}

# interval-mean-stdev --
#    Return the interval containing the mean value and one
#    containing the standard deviation with a certain
#    level of confidence (assuming a normal distribution)
#
# Arguments:
#    data         List of raw data values
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    List having the following elements: lower and upper bounds of
#    mean, lower and upper bounds of stdev
#
#
proc ::math::statistics::interval-mean-stdev { data confidence } {
    variable TOOFEWDATA
    variable student_t_table

    set allstats [BasicStats all $data]

    set conf2    [expr {(1.0+$confidence)/2.0}]
    set mean     [lindex $allstats 0]
    set number   [lindex $allstats 3]
    set stdev    [lindex $allstats 4]

    if { $number > 1 } {
	set degrees    [expr {$number-1}]
	set student_t \
		[::math::interpolation::interpolate2d $student_t_table \
		$degrees $conf2]
	set mean_lower [expr {$mean-$student_t*$stdev/sqrt($number)}]
	set mean_upper [expr {$mean+$student_t*$stdev/sqrt($number)}]
	set stdev_lower {}
	set stdev_upper {}
	return [list $mean_lower $mean_upper $stdev_lower $stdev_upper]
    } else {
	return -code error -errorcode DATA -errorinfo $TOOFEWDATA $TOOFEWDATA
    }
}

# quantiles --
#    Return the quantiles for a given set of data or histogram
#
# Arguments:
#    (two arguments)
#    data         List of raw data values
#    confidence   Confidence level (0.95 or 0.99 for instance)
#    (three arguments)
#    limits       List of upper limits from histogram
#    counts       List of counts for for each interval in histogram
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    List of quantiles
#
proc ::math::statistics::quantiles { arg1 arg2 {arg3 {}} } {
    variable TOOFEWDATA

    if { [catch {
	if { $arg3 == {} } {
	    set result \
		    [::math::statistics::QuantilesRawData $arg1 $arg2]
	} else {
	    set result \
		    [::math::statistics::QuantilesHistogram $arg1 $arg2 $arg3]
	}
    } msg] } {
	return -code error -errorcode $msg $msg
    }
    return $result
}

# QuantilesRawData --
#    Return the quantiles based on raw data
#
# Arguments:
#    data         List of raw data values
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    List of quantiles
#
proc ::math::statistics::QuantilesRawData { data confidence } {
    variable TOOFEWDATA
    variable OUTOFRANGE

    if { [llength $confidence] <= 0 } {
	return -code error -errorcode ARG "$TOOFEWDATA - quantiles"
    }

    if { [llength $data] <= 0 } {
	return -code error -errorcode ARG "$TOOFEWDATA - raw data"
    }

    foreach cond $confidence {
	if { $cond <= 0.0 || $cond >= 1.0 } {
	    return -code error -errorcode ARG "$OUTOFRANGE - quantiles"
	}
    }

    #
    # Sort the data first
    #
    set sorted_data [lsort -real -increasing $data]

    #
    # Determine the list element lower or equal to the quantile
    # and return the corresponding value
    #
    set result      {}
    set number_data [llength $sorted_data]
    foreach cond $confidence {
	set elem [expr {round($number_data*$cond)-1}]
	if { $elem < 0 } {
	    set elem 0
	}
	lappend result [lindex $sorted_data $elem]
    }

    return $result
}

# QuantilesHistogram --
#    Return the quantiles based on histogram information only
#
# Arguments:
#    limits       Upper limits for histogram intervals
#    counts       Counts for each interval
#    confidence   Confidence level (0.95 or 0.99 for instance)
#
# Result:
#    List of quantiles
#
proc ::math::statistics::QuantilesHistogram { limits counts confidence } {
    variable TOOFEWDATA
    variable OUTOFRANGE

    if { [llength $confidence] <= 0 } {
	return -code error -errorcode ARG "$TOOFEWDATA - quantiles"
    }

    if { [llength $confidence] <= 0 } {
	return -code error -errorcode ARG "$TOOFEWDATA - histogram limits"
    }

    if { [llength $counts] <= [llength $limits] } {
	return -code error -errorcode ARG "$TOOFEWDATA - histogram counts"
    }

    foreach cond $confidence {
	if { $cond <= 0.0 || $cond >= 1.0 } {
	    return -code error -errorcode ARG "$OUTOFRANGE - quantiles"
	}
    }

    #
    # Accumulate the histogram counts first
    #
    set sum 0
    set accumulated_counts {}
    foreach count $counts {
	set sum [expr {$sum+$count}]
	lappend accumulated_counts $sum
    }
    set total_counts $sum

    #
    # Determine the list element lower or equal to the quantile
    # and return the corresponding value (use interpolation if
    # possible)
    #
    set result      {}
    foreach cond $confidence {
	set found       0
	set bound       [expr {round($total_counts*$cond)}]
	set lower_limit {}
	set lower_count 0
	foreach acc_count $accumulated_counts limit $limits {
	    if { $acc_count >= $bound } {
		set found 1
		break
	    }
	    set lower_limit $limit
	    set lower_count $acc_count
	}

	if { $lower_limit == {} || $limit == {} || $found == 0 } {
	    set quant $limit
	    if { $limit == {} } {
		set quant $lower_limit
	    }
	} else {
	    set quant [expr {$limit+($lower_limit-$limit) *
	    ($acc_count-$bound)/($acc_count-$lower_count)}]
	}
	lappend result $quant
    }

    return $result
}

# autocorr --
#    Return the autocorrelation function (assuming equidistance between
#    samples)
#
# Arguments:
#    data         Raw data for which the autocorrelation must be determined
#
# Result:
#    List of autocorrelation values (about 1/2 the number of raw data)
#
proc ::math::statistics::autocorr { data } {
    variable TOOFEWDATA

    if { [llength $data] <= 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA"
    }

    return [crosscorr $data $data]
}

# crosscorr --
#    Return the cross-correlation function (assuming equidistance
#    between samples)
#
# Arguments:
#    data1        First set of raw data
#    data2        Second set of raw data
#
# Result:
#    List of cross-correlation values (about 1/2 the number of raw data)
#
# Note:
#    The number of data pairs is not kept constant - because tests
#    showed rather awkward results when it was kept constant.
#
proc ::math::statistics::crosscorr { data1 data2 } {
    variable TOOFEWDATA

    if { [llength $data1] <= 1 || [llength $data2] <= 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA"
    }

    #
    # First determine the number of data pairs
    #
    set number1 [llength $data1]
    set number2 [llength $data2]

    set basic_stat1 [basic-stats $data1]
    set basic_stat2 [basic-stats $data2]
    set vmean1      [lindex $basic_stat1 0]
    set vmean2      [lindex $basic_stat2 0]
    set vvar1       [lindex $basic_stat1 end]
    set vvar2       [lindex $basic_stat2 end]

    set number_pairs $number1
    if { $number1 > $number2 } {
	set number_pairs $number2
    }
    set number_values $number_pairs
    set number_delays [expr {$number_values/2.0}]

    set scale [expr {sqrt($vvar1*$vvar2)}]

    set result {}
    for { set delay 0 } { $delay < $number_delays } { incr delay } {
	set sumcross 0.0
	set no_cross 0
	for { set idx 0 } { $idx < $number_values } { incr idx } {
	    set value1 [lindex $data1 $idx]
	    set value2 [lindex $data2 [expr {$idx+$delay}]]
	    if { $value1 != {} && $value2 != {} } {
		set  sumcross \
			[expr {$sumcross+($value1-$vmean1)*($value2-$vmean2)}]
		incr no_cross
	    }
	}
	lappend result [expr {$sumcross/($no_cross*$scale)}]

	incr number_values -1
    }

    return $result
}

# mean-histogram-limits
#    Determine reasonable limits based on mean and standard deviation
#    for a histogram
#
# Arguments:
#    mean         Mean of the data
#    stdev        Standard deviation
#    number       Number of limits to generate (defaults to 8)
#
# Result:
#    List of limits
#
proc ::math::statistics::mean-histogram-limits { mean stdev {number 8} } {
    variable NEGSTDEV

    if { $stdev <= 0.0 } {
	return -code error -errorcode ARG "$NEGSTDEV"
    }
    if { $number < 1 } {
	return -code error -errorcode ARG "Number of limits must be positive"
    }

    #
    # Always: between mean-3.0*stdev and mean+3.0*stdev
    # number = 2: -0.25, 0.25
    # number = 3: -0.25, 0, 0.25
    # number = 4: -1, -0.25, 0.25, 1
    # number = 5: -1, -0.25, 0, 0.25, 1
    # number = 6: -2, -1, -0.25, 0.25, 1, 2
    # number = 7: -2, -1, -0.25, 0, 0.25, 1, 2
    # number = 8: -3, -2, -1, -0.25, 0.25, 1, 2, 3
    #
    switch -- $number {
	"1" { set limits {0.0} }
	"2" { set limits {-0.25 0.25} }
	"3" { set limits {-0.25 0.0 0.25} }
	"4" { set limits {-1.0 -0.25 0.25 1.0} }
	"5" { set limits {-1.0 -0.25 0.0 0.25 1.0} }
	"6" { set limits {-2.0 -1.0 -0.25 0.25 1.0 2.0} }
	"7" { set limits {-2.0 -1.0 -0.25 0.0 0.25 1.0 2.0} }
	"8" { set limits {-3.0 -2.0 -1.0 -0.25 0.25 1.0 2.0 3.0} }
	"9" { set limits {-3.0 -2.0 -1.0 -0.25 0.0 0.25 1.0 2.0 3.0} }
	default {
	    set dlim [expr {6.0/double($number-1)}]
	    for {set i 0} {$i <$number} {incr i} {
		lappend limits [expr {$dlim*($i-($number-1)/2.0)}]
	    }
	}
    }

    set result {}
    foreach limit $limits {
	lappend result [expr {$mean+$limit*$stdev}]
    }

    return $result
}

# minmax-histogram-limits
#    Determine reasonable limits based on minimum and maximum bounds
#    for a histogram
#
# Arguments:
#    min          Estimated minimum
#    max          Estimated maximum
#    number       Number of limits to generate (defaults to 8)
#
# Result:
#    List of limits
#
proc ::math::statistics::minmax-histogram-limits { min max {number 8} } {
    variable NEGSTDEV

    if { $number < 1 } {
	return -code error -errorcode ARG "Number of limits must be positive"
    }
    if { $min >= $max } {
	return -code error -errorcode ARG "Minimum must be lower than maximum"
    }

    set result {}
    set dlim [expr {($max-$min)/double($number-1)}]
    for {set i 0} {$i <$number} {incr i} {
	lappend result [expr {$min+$dlim*$i}]
    }

    return $result
}

# linear-model
#    Determine the coefficients for a linear regression between
#    two series of data
#
# Arguments:
#    xdata        Series of independent (X) data
#    ydata        Series of dependent (Y) data
#
# Result:
#    List of the following items:
#    PM
#
proc ::math::statistics::linear-model { xdata ydata } {
    variable TOOFEWDATA

    if { [llength $xdata] < 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA: no independent data"
    }
    if { [llength $ydata] < 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA: no dependent data"
    }

    # PM
}

# linear-residuals
#    Determine the difference between actual data and predicted from
#    the linear model
#
# Arguments:
#    xdata        Series of independent (X) data
#    ydata        Series of dependent (Y) data
#
# Result:
#    List of differences
#
proc ::math::statistics::linear-residuals { xdata ydata } {
    variable TOOFEWDATA

    if { [llength $xdata] < 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA: no independent data"
    }
    if { [llength $ydata] < 1 } {
	return -code error -errorcode ARG "$TOOFEWDATA: no dependent data"
    }

    # PM
}

#
# Load the auxiliary scripts
#
source [file join [file dirname [info script]] pdf_stat.tcl]
source [file join [file dirname [info script]] plotstat.tcl]
source [file join [file dirname [info script]] liststat.tcl]

#
# Define the tables
#
namespace eval ::math::statistics {
    variable student_t_table

    #   set student_t_table [::math::interpolation::defineTable student_t
    #          {X        80%    90%    95%    98%    99%}
    #          {X      0.80   0.90   0.95   0.98   0.99
    #           1      3.078  6.314 12.706 31.821 63.657
    #           2      1.886  2.920  4.303  6.965  9.925
    #           3      1.638  2.353  3.182  4.541  5.841
    #           5      1.476  2.015  2.571  3.365  4.032
    #          10      1.372  1.812  2.228  2.764  3.169
    #          15      1.341  1.753  2.131  2.602  2.947
    #          20      1.325  1.725  2.086  2.528  2.845
    #          30      1.310  1.697  2.042  2.457  2.750
    #          60      1.296  1.671  2.000  2.390  2.660
    #         1.0e9    1.282  1.645  1.960  2.326  2.576 }]

    # PM
    #set chi_squared_table [::math::interpolation::defineTable chi_square
    #   ...
}

#
# Simple test code
#
if { [file tail [info script]] == [file tail $::argv0] } {

    console show
    puts [interp aliases]

    set values {1 1 1 1 {}}
    puts [::math::statistics::basic-stats $values]
    set values {1 2 3 4}
    puts [::math::statistics::basic-stats $values]
    set values {1 -1 1 -2}
    puts [::math::statistics::basic-stats $values]
    puts [::math::statistics::mean   $values]
    puts [::math::statistics::min    $values]
    puts [::math::statistics::max    $values]
    puts [::math::statistics::number $values]
    puts [::math::statistics::stdev  $values]
    puts [::math::statistics::var    $values]

    set novals 100
    #set maxvals 100001
    set maxvals 1001
    while { $novals < $maxvals } {
	set values {}
	for { set i 0 } { $i < $novals } { incr i } {
	    lappend values [expr {rand()}]
	}
	puts [::math::statistics::basic-stats $values]
	puts [::math::statistics::histogram {0.0 0.2 0.4 0.6 0.8 1.0} $values]
	set novals [expr {$novals*10}]
    }

    puts "Normal distribution:"
    puts "X=0:  [::math::statistics::pdf-normal 0.0 1.0 0.0]"
    puts "X=1:  [::math::statistics::pdf-normal 0.0 1.0 1.0]"
    puts "X=-1: [::math::statistics::pdf-normal 0.0 1.0 -1.0]"

    set data1 {0.0 1.0 3.0 4.0 100.0 -23.0}
    set data2 {1.0 2.0 4.0 5.0 101.0 -22.0}
    set data3 {0.0 2.0 6.0 8.0 200.0 -46.0}
    set data4 {2.0 6.0 8.0 200.0 -46.0 1.0}
    set data5 {100.0 99.0 90.0 93.0 5.0 123.0}
    puts "Correlation data1 and data1: [::math::statistics::corr $data1 $data1]"
    puts "Correlation data1 and data2: [::math::statistics::corr $data1 $data2]"
    puts "Correlation data1 and data3: [::math::statistics::corr $data1 $data3]"
    puts "Correlation data1 and data4: [::math::statistics::corr $data1 $data4]"
    puts "Correlation data1 and data5: [::math::statistics::corr $data1 $data5]"

    #   set data {1.0 2.0 2.3 4.0 3.4 1.2 0.6 5.6}
    #   puts [::math::statistics::basicStats $data]
    #   puts [::math::statistics::interval-mean-stdev $data 0.90]
    #   puts [::math::statistics::interval-mean-stdev $data 0.95]
    #   puts [::math::statistics::interval-mean-stdev $data 0.99]

    #   puts "\nTest mean values:"
    #   puts [::math::statistics::test-mean $data 2.0 0.1 0.90]
    #   puts [::math::statistics::test-mean $data 2.0 0.5 0.90]
    #   puts [::math::statistics::test-mean $data 2.0 1.0 0.90]
    #   puts [::math::statistics::test-mean $data 2.0 2.0 0.90]

    set rc [catch {
	set m [::math::statistics::mean {}]
    } msg ] ; # {}
    puts "Result: $rc $msg"

    puts "\nTest quantiles:"
    set data      {1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0 10.0}
    set quantiles {0.11 0.21 0.51 0.91 0.99}
    set limits    {2.1 4.1 6.1 8.1}
    puts [::math::statistics::quantiles $data $quantiles]

    set histogram [::math::statistics::histogram $limits $data]
    puts [::math::statistics::quantiles $limits $histogram $quantiles]

    puts "\nTest autocorrelation:"
    set data      {1.0 -1.0 1.0 -1.0 1.0 -1.0 1.0 -1.0 1.0}
    puts [::math::statistics::autocorr $data]
    set data      {1.0 -1.1 2.0 -0.6 3.0 -4.0 0.5 0.9 -1.0}
    puts [::math::statistics::autocorr $data]

    puts "\nTest histogram limits:"
    puts [::math::statistics::mean-histogram-limits   1.0 1.0]
    puts [::math::statistics::mean-histogram-limits   1.0 1.0 4]
    puts [::math::statistics::minmax-histogram-limits 1.0 10.0 10]

}
