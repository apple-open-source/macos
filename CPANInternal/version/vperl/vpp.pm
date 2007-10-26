
package version::vpp;
use strict;

use Exporter ();
use Scalar::Util qw(isvstring reftype);
use vars qw ($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS @REGEXS);
$VERSION     = $version::VERSION;
@ISA         = qw (Exporter);
#Give a hoot don't pollute, do not export more than needed by default
@EXPORT      = qw (qv);
@EXPORT_OK   = qw ();
%EXPORT_TAGS = ();

push @REGEXS, qr/
	^v?	# optional leading 'v'
	(\d*)	# major revision not required
	\.	# requires at least one decimal
	(?:(\d+)\.?){1,}
	/x;

local $^W; # shut up the 'redefined' warning for UNIVERSAL::VERSION

use overload (
    '+0'   => \&numify,
    '""'   => \&stringify,
    'cmp'  => \&vcmp,
    '<=>'  => \&vcmp,
);

sub new
{
	my ($class, $value) = @_;
	my $self = bless ({}, ref ($class) || $class);

	if ( $#_ == 2 ) { # must be CVS-style
	    $value = 'v'.$_[2];
	}

	if ( isvstring($value) ) {
	    $value = sprintf("v%vd",$value);
	}
	
	# This is not very efficient, but it is morally equivalent
	# to the XS code (as that is the reference implementation).
	# See vutil/vutil.c for details
	my $qv = 0;
	my $alpha = 0;
	my $width = 3;
	my $saw_period = 0;
	my ($start, $last, $pos, $s);
	$s = 0;

	while ( substr($value,$s,1) =~ /\s/ ) { # leading whitespace is OK
	    $s++;
	}

	if (substr($value,$s,1) eq 'v') {
	    $s++;    # get past 'v'
	    $qv = 1; # force quoted version processing
	}

	$start = $last = $pos = $s;
		
	# pre-scan the input string to check for decimals/underbars
	while ( substr($value,$pos,1) =~ /[._\d]/ ) {
	    if ( substr($value,$pos,1) eq '.' ) {
		die "Invalid version format (underscores before decimal)"
		  if $alpha;
		$saw_period++;
		$last = $pos;
	    }
	    elsif ( substr($value,$pos,1) eq '_' ) {
		die "Invalid version format (multiple underscores)"
		  if $alpha;
		$alpha = 1;
		$width = $pos - $last - 1; # natural width of sub-version
	    }
	    $pos++;
	}

	if ( $alpha && !$saw_period ) {
	    die "Invalid version format (alpha without decimal)";
	}

	if ( $saw_period > 1 ) {
	    $qv = 1; # force quoted version processing
	}

	$pos = $s;

	if ( $qv ) {
	    $self->{qv} = 1;
	}

	if ( $alpha ) {
	    $self->{alpha} = 1;
	}

	if ( !$qv && $width < 3 ) {
	    $self->{width} = $width;
	}

	while ( substr($value,$pos,1) =~ /\d/ ) {
	    $pos++;
	}

	if ( substr($value,$pos,1) !~ /[a-z]/ ) { ### FIX THIS ###
	    my $rev;

	    while (1) {
		$rev = 0;
		{

		    # this is atoi() that delimits on underscores
		    my $end = $pos;
		    my $mult = 1;
		    my $orev;

		    # the following if() will only be true after the decimal
		    # point of a version originally created with a bare
		    # floating point number, i.e. not quoted in any way
		    if ( !$qv && $s > $start && $saw_period == 1 ) {
			$mult *= 100;
			while ( $s < $end ) {
			    $orev = $rev;
			    $rev += substr($value,$s,1) * $mult;
			    $mult /= 10;
			    if ( abs($orev) > abs($rev) ) {
				die "Integer overflow in version";
			    }
			    $s++;
			    if ( substr($value,$s,1) eq '_' ) {
				$s++;
			    }
			}
		    }
		    else {
			while (--$end >= $s) {
			    $orev = $rev;
			    $rev += substr($value,$end,1) * $mult;
			    $mult *= 10;
			    if ( abs($orev) > abs($rev) ) {
				die "Integer overflow in version";
			    }
			}
		    }
		}

		# Append revision
		push @{$self->{version}}, $rev;
		if ( substr($value,$pos,1) eq '.' 
		    && substr($value,$pos+1,1) =~ /\d/ ) {
		    $s = ++$pos;
		}
		elsif ( substr($value,$pos,1) eq '_' 
		    && substr($value,$pos+1,1) =~ /\d/ ) {
		    $s = ++$pos;
		}
		elsif ( substr($value,$pos,1) =~ /\d/ ) {
		    $s = $pos;
		}
		else {
		    $s = $pos;
		    last;
		}
		if ( $qv ) {
		    while ( substr($value,$pos,1) =~ /\d/ ) {
			$pos++;
		    }
		}
		else {
		    my $digits = 0;
		    while (substr($value,$pos,1) =~ /[\d_]/ && $digits < 3) {
			if ( substr($value,$pos,1) ne '_' ) {
			    $digits++;
			}
			$pos++;
		    }
		}
	    }
	}
	if ( $qv ) { # quoted versions always get at least three terms
	    my $len = scalar @{$self->{version}};
	    $len = 3 - $len;
	    while ($len-- > 0) {
		push @{$self->{version}}, 0;
	    }
	}

	if ( not exists $self->{version} ) {
	    # oops, someone forgot to pass a value (shouldn't happen)
	    push @{$self->{version}}, 0;
	}

	if ( substr($value,$pos) ) { # any remaining text
	    warn "Version string '$value' contains invalid data; ".
	         "ignoring: '".substr($value,$pos)."'";
	}

	return ($self);
}

sub numify 
{
    my ($self) = @_;
    unless (_verify($self)) {
	die "Invalid version object";
    }
    my $width = $self->{width} || 3;
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("%d.", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
	$digit = $self->{version}[$i];
	if ( $width < 3 ) {
	    my $denom = 10**(3-$width);
	    my $quot = int($digit/$denom);
	    my $rem = $digit - ($quot * $denom);
	    $string .= sprintf("%0".$width."d_%d", $quot, $rem);
	}
	else {
	    $string .= sprintf("%03d", $digit);
	}
    }

    if ( $len > 0 ) {
	$digit = $self->{version}[$len];
	if ( $alpha && $width == 3 ) {
	    $string .= "_";
	}
	$string .= sprintf("%0".$width."d", $digit);
    }
    else # $len = 0
    {
	$string .= sprintf("000");
    }

    return $string;
}

sub normal 
{
    my ($self) = @_;
    unless (_verify($self)) {
	die "Invalid version object";
    }
    my $alpha = $self->{alpha} || "";
    my $len = $#{$self->{version}};
    my $digit = $self->{version}[0];
    my $string = sprintf("v%d", $digit );

    for ( my $i = 1 ; $i < $len ; $i++ ) {
	$digit = $self->{version}[$i];
	$string .= sprintf(".%d", $digit);
    }

    if ( $len > 0 ) {
	$digit = $self->{version}[$len];
	if ( $alpha ) {
	    $string .= sprintf("_%0d", $digit);
	}
	else {
	    $string .= sprintf(".%0d", $digit);
	}
    }

    if ( $len <= 2 ) {
	for ( $len = 2 - $len; $len != 0; $len-- ) {
	    $string .= sprintf(".%0d", 0);
	}
    }

    return $string;
}

sub stringify
{
    my ($self) = @_;
    unless (_verify($self)) {
	die "Invalid version object";
    }
    if ( exists $self->{qv} ) {
	return $self->normal;
    }
    else {
	return $self->numify;
    }
}

sub vcmp
{
    require UNIVERSAL;
    my ($left,$right,$swap) = @_;
    my $class = ref($left);
    unless ( UNIVERSAL::isa($right, $class) ) {
	$right = $class->new($right);
    }

    if ( $swap ) {
	($left, $right) = ($right, $left);
    }
    unless (_verify($left)) {
	die "Invalid version object";
    }
    unless (_verify($right)) {
	die "Invalid version object";
    }
    my $l = $#{$left->{version}};
    my $r = $#{$right->{version}};
    my $m = $l < $r ? $l : $r;
    my $lalpha = $left->is_alpha;
    my $ralpha = $right->is_alpha;
    my $retval = 0;
    my $i = 0;
    while ( $i <= $m && $retval == 0 ) {
	$retval = $left->{version}[$i] <=> $right->{version}[$i];
	$i++;
    }

    # tiebreaker for alpha with identical terms
    if ( $retval == 0 
	&& $l == $r 
	&& $left->{version}[$m] == $right->{version}[$m]
	&& ( $lalpha || $ralpha ) ) {

	if ( $lalpha && !$ralpha ) {
	    $retval = -1;
	}
	elsif ( $ralpha && !$lalpha) {
	    $retval = +1;
	}
    }

    # possible match except for trailing 0's
    if ( $retval == 0 && $l != $r ) {
	if ( $l < $r ) {
	    while ( $i <= $r && $retval == 0 ) {
		if ( $right->{version}[$i] != 0 ) {
		    $retval = -1; # not a match after all
		}
		$i++;
	    }
	}
	else {
	    while ( $i <= $l && $retval == 0 ) {
		if ( $left->{version}[$i] != 0 ) {
		    $retval = +1; # not a match after all
		}
		$i++;
	    }
	}
    }

    return $retval;  
}

sub is_alpha {
    my ($self) = @_;
    return (exists $self->{alpha});
}

sub qv {
    my ($value) = @_;

    if ( isvstring($value) ) {
	$value = sprintf("v%vd",$value);
    }
    else {
	$value = 'v'.$value unless $value =~ /^v/;
    }
    return version->new($value); # always use base class
}

sub _verify {
    my ($self) = @_;
    if (   reftype($self) eq 'HASH'
	&& exists $self->{version}
	&& ref($self->{version}) eq 'ARRAY'
	) {
	return 1;
    }
    else {
	return 0;
    }
}

1; #this line is important and will help the module return a true value
