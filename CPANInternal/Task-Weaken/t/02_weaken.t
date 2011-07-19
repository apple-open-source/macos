#!/usr/bin/perl

# Ensure that this platform really does have weak references and weaken.

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 19;
use File::Spec::Functions ':ALL';
use Scalar::Util ();

# Ensure we can import weaken and isweak
BEGIN {
	Scalar::Util->import( 'weaken' );
	Scalar::Util->import( 'isweak' );
}
ok( defined(&weaken), '->import(weaken) worked' );
ok( defined(&isweak), '->import(isweak) worked' );





#####################################################################
# Functional Tests

# Test weaken, just in case someone tries to fake its existance.
# Code copied from Scalar::Util itself and stripped of non-essentials.
SCOPE: {
	package Dest;
	sub DESTROY {
		${$_[0]{Flag}} ++;
	}
}
my ($y,$z);
SCOPE: {
	my $x = "foo";
	$y = \$x;
	$z = \$x;
}
ok( ref($y) and ref($z));
weaken($y);
ok( ref($y) and ref($z));
undef($z);
ok( not (defined($y) and defined($z)) );
undef($y);
ok( not (defined($y) and defined($z)) );
SCOPE: {
	my $x = "foo";
	$y = \$x;
}
ok( ref($y) );
weaken($y);
ok( not defined $y  );
my $flag = 0;
SCOPE: {
	my $y = bless {}, 'Dest';
	$y->{Self} = $y;
	$y->{Flag} = \$flag;
	weaken($y->{Self});
	ok( ref($y) );
}
ok( $flag == 1 );
undef $flag;
$flag = 0;
{
	my $y = bless {}, 'Dest';
	my $x = bless {}, 'Dest';
	$x->{Ref} = $y;
	$y->{Ref} = $x;
	$x->{Flag} = \$flag;
	$y->{Flag} = \$flag;
	weaken($x->{Ref});
}
ok( $flag == 2 );
SCOPE: {
	my $x = "foo";
	$y = \$x;
	$z = \$x;
}
weaken($y);
undef($y);
ok( not defined $y);
ok( ref($z) );
my $a = 5;
ok(!isweak($a));
my $b = \$a;
ok(!isweak($b));
weaken($b);
ok(isweak($b));
$b = \$a;
ok(!isweak($b));

my $x = {};
weaken($x->{Y} = \$a);
ok(isweak($x->{Y}));
ok(!isweak($x->{Z}));
