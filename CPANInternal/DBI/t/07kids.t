#!perl -w
$|=1;

use strict;

use Test::More;

use DBI 1.50; # also tests Exporter::require_version

BEGIN {
	plan skip_all => '$h->{Kids} attribute not supported for DBI::PurePerl'
		if $DBI::PurePerl && $DBI::PurePerl; # doubled to avoid typo warning
	plan tests => 20;
}

## ----------------------------------------------------------------------------
## 07kids.t
## ----------------------------------------------------------------------------
# This test check the Kids and the ActiveKids attributes and how they act
# in various situations.
#
# Check the database handle's kids:
#   - upon creation of handle
#   - upon creation of statement handle
#   - after execute of statement handle
#   - after finish of statement handle
#   - after destruction of statement handle
# Check the driver handle's kids:
#   - after creation of database handle
#   - after disconnection of database handle
#   - after destruction of database handle
## ----------------------------------------------------------------------------


# Connect to the example driver and create a database handle
my $dbh = DBI->connect('dbi:ExampleP:dummy', '', '',
						   { 
							 PrintError => 1,
							 RaiseError => 0
						   });

# check our database handle to make sure its good
isa_ok($dbh, 'DBI::db');

# check that it has no Kids or ActiveKids yet
cmp_ok($dbh->{Kids}, '==', 0, '... database handle has 0 Kid(s) at start');
cmp_ok($dbh->{ActiveKids}, '==', 0, '... database handle has 0 ActiveKid(s) at start');

# create a scope for our $sth to live and die in
do { 

	# create a statement handle
	my $sth = $dbh->prepare('select uid from ./');

	# verify that it is a correct statement handle
	isa_ok($sth, "DBI::st");

	# check our Kids and ActiveKids after prepare
	cmp_ok($dbh->{Kids}, '==', 1, '... database handle has 1 Kid(s) after $dbh->prepare');
	cmp_ok($dbh->{ActiveKids}, '==', 0, '... database handle has 0 ActiveKid(s) after $dbh->prepare');

	$sth->execute();

	# check our Kids and ActiveKids after execute
	cmp_ok($dbh->{Kids}, '==', 1, '... database handle has 1 Kid(s) after $sth->execute');
	cmp_ok($dbh->{ActiveKids}, '==', 1, '... database handle has 1 ActiveKid(s) after $sth->execute');

	$sth->finish();

	# check our Kids and Activekids after finish
	cmp_ok($dbh->{Kids}, '==', 1, '... database handle has 1 Kid(s) after $sth->finish');
	cmp_ok($dbh->{ActiveKids}, '==', 0, '... database handle has 0 ActiveKid(s) after $sth->finish');

};

# now check it after the statement handle has been destroyed
cmp_ok($dbh->{Kids}, '==', 0, '... database handle has 0 Kid(s) after $sth is destroyed');
cmp_ok($dbh->{ActiveKids}, '==', 0, '... database handle has 0 ActiveKid(s) after $sth is destroyed');

# get the database handles driver Driver
my $drh = $dbh->{Driver};

# check that is it a correct driver handle
isa_ok($drh, "DBI::dr");

# check the driver's Kids and ActiveKids 
cmp_ok( $drh->{Kids}, '==', 1, '... driver handle has 1 Kid(s)');
cmp_ok( $drh->{ActiveKids}, '==', 1, '... driver handle has 1 ActiveKid(s)');

$dbh->disconnect;

# check the driver's Kids and ActiveKids after $dbh->disconnect
cmp_ok( $drh->{Kids}, '==', 1, '... driver handle has 1 Kid(s) after $dbh->disconnect');
cmp_ok( $drh->{ActiveKids}, '==', 0, '... driver handle has 0 ActiveKid(s) after $dbh->disconnect');

undef $dbh;
ok(!defined($dbh), '... lets be sure that $dbh is not undefined');

# check the driver's Kids and ActiveKids after undef $dbh
cmp_ok( $drh->{Kids}, '==', 0, '... driver handle has 0 Kid(s) after undef $dbh');
cmp_ok( $drh->{ActiveKids}, '==', 0, '... driver handle has 0 ActiveKid(s) after undef $dbh');

