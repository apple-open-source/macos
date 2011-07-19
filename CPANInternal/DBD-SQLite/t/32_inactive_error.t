#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 4;
use t::lib::Test;

my $dbh = connect_ok( PrintError => 0, RaiseError => 0 );

my $sth = $dbh->prepare('CREATE TABLE foo (f)');

$dbh->disconnect;

$sth->{PrintError} = 1;

# attempt to execute on inactive database handle
my @warning = ();
SCOPE: {
	local $SIG{__WARN__} = sub { push @warning, @_; return };
	my $ret = eval { $sth->execute; };
	# we need PrintError => 1, or warn $@ if $@;
	ok ! defined $ret;
}

is( scalar(@warning), 1, 'Got 1 warning' );
like(
	$warning[0],
	qr/attempt to execute on inactive database handle/,
	'Got the expected warning',
);
