#!perl -w
# vim:sw=4:ts=8
$|=1;

use strict;

use Test::More tests => 51;

## ----------------------------------------------------------------------------
## 02dbidrv.t - ...
## ----------------------------------------------------------------------------
# This test creates a Test Driver (DBD::Test) and then exercises it.
# NOTE:
# There are a number of tests as well that are embedded within the actual
# driver code as well
## ----------------------------------------------------------------------------

## load DBI

BEGIN {
	use_ok('DBI');
}

## ----------------------------------------------------------------------------
## create a Test Driver (DBD::Test)

## main Test Driver Package
{   
    package DBD::Test;

    use strict;
    use warnings;

    my $drh = undef;

    sub driver {
		return $drh if $drh;
		
		Test::More::pass('... DBD::Test->driver called to getnew Driver handle');
		
		my($class, $attr) = @_;
		$class = "${class}::dr";
		($drh) = DBI::_new_drh($class, {
							Name    => 'Test',
							Version => '$Revision: 11.11 $',
						},
					77	# 'implementors data'
					);
			
		Test::More::ok($drh, "... new Driver handle ($drh) created successfully");
		Test::More::isa_ok($drh, 'DBI::dr');
		
		return $drh;
    }
}

## Test Driver
{   
	package DBD::Test::dr;
	
    use strict;
	use warnings;
    
	$DBD::Test::dr::imp_data_size = 0;
	
    Test::More::cmp_ok($DBD::Test::dr::imp_data_size, '==', 0, '... check DBD::Test::dr::imp_data_size to avoid typo');

    sub DESTROY { undef }

    sub data_sources {
		my ($h) = @_;
		
		Test::More::ok($h, '... Driver object passed to data_sources');
		Test::More::isa_ok($h, 'DBI::dr');
		Test::More::ok(!tied $h, '... Driver object is not tied');
		
		return ("dbi:Test:foo", "dbi:Test:bar");
    }
}

## Test db package
{   
	package DBD::Test::db;
    
	use strict;
    
	$DBD::Test::db::imp_data_size = 0;
	
    Test::More::cmp_ok($DBD::Test::db::imp_data_size, '==', 0, '... check DBD::Test::db::imp_data_size to avoid typo');

    sub do {
		my $h = shift;

		Test::More::ok($h, '... Database object passed to do');
		Test::More::isa_ok($h, 'DBI::db');
		Test::More::ok(!tied $h, '... Database object is not tied');

		my $drh_i = $h->{Driver};
		
		Test::More::ok($drh_i, '... got Driver object from Database object with Driver attribute');
		Test::More::isa_ok($drh_i, "DBI::dr");
		Test::More::ok(!tied %{$drh_i}, '... Driver object is not tied');

		my $drh_o = $h->FETCH('Driver');
		
		Test::More::ok($drh_o, '... got Driver object from Database object by FETCH-ing Driver attribute');
		Test::More::isa_ok($drh_o, "DBI::dr");
		SKIP: {
			Test::More::skip "running DBI::PurePerl", 1 if $DBI::PurePerl;
			Test::More::ok(tied %{$drh_o}, '... Driver object is not tied');
		}
		
		# return this to make our test pass
		return 1;
    }

    sub data_sources {	
		my ($dbh, $attr) = @_;
		my @ds = $dbh->SUPER::data_sources($attr);
		
		Test::More::is_deeply((
				\@ds,
				[ 'dbi:Test:foo', 'dbi:Test:bar' ]
				), 
			'... checking fetched datasources from Driver'
			);
		
		push @ds, "dbi:Test:baz";
		return @ds;
    }

    sub disconnect {
	shift->STORE(Active => 0);
    }
}

## ----------------------------------------------------------------------------
## test the Driver (DBD::Test)

$INC{'DBD/Test.pm'} = 'dummy';	# required to fool DBI->install_driver()

# Note that install_driver should *not* normally be called directly.
# This test does so only because it's a test of install_driver!

my $drh = DBI->install_driver('Test');

ok($drh, '... got a Test Driver object back from DBI->install_driver');
isa_ok($drh, 'DBI::dr');

cmp_ok(DBI::_get_imp_data($drh), '==', 77, '... checking the DBI::_get_imp_data function');

my @ds1 = DBI->data_sources("Test");
is_deeply((
	[ @ds1 ],
	[ 'dbi:Test:foo', 'dbi:Test:bar' ]
	), '... got correct datasources from DBI->data_sources("Test")'
);

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh->{Kids}, '==', 0, '... this Driver does not yet have any Kids');
}

# create scope to test $dbh DESTROY behaviour
do {				

	my $dbh = $drh->connect;
	
	ok($dbh, '... got a database handle from calling $drh->connect');
	isa_ok($dbh, 'DBI::db');

    SKIP: {
        skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
        cmp_ok($drh->{Kids}, '==', 1, '... this Driver does not yet have any Kids');
    }  

	my @ds2 = $dbh->data_sources();
	is_deeply((
		[ @ds2 ],
		[ 'dbi:Test:foo', 'dbi:Test:bar', 'dbi:Test:baz' ]
		), '... got correct datasources from $dbh->data_sources()'
	);
	
	ok($dbh->do('dummy'), '... this will trigger more driver internal tests above in DBD::Test::db');

	$dbh->disconnect;

	$drh->set_err("41", "foo 41 drh");
	cmp_ok($drh->err, '==', 41, '... checking Driver handle err set with set_err method');
	$dbh->set_err("42", "foo 42 dbh");
	cmp_ok($dbh->err, '==', 42, '... checking Database handle err set with set_err method');
	cmp_ok($drh->err, '==', 41, '... checking Database handle err set with Driver handle set_err method');

};

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh->{Kids}, '==', 0, '... this Driver does not yet have any Kids')
        or $drh->dump_handle("bad Kids",3);
}

# copied up to drh from dbh when dbh was DESTROYd
cmp_ok($drh->err, '==', 42, '... $dbh->DESTROY should set $drh->err to 42');

$drh->set_err("99", "foo");
cmp_ok($DBI::err, '==', 99, '... checking $DBI::err set with Driver handle set_err method');
is($DBI::errstr, "foo 42 dbh [err was 42 now 99]\nfoo", '... checking $DBI::errstr');

$drh->default_user("",""); # just to reset err etc
$drh->set_err(1, "errmsg", "00000");
is($DBI::state, "", '... checking $DBI::state');

$drh->set_err(1, "test error 1");
is($DBI::state, 'S1000', '... checking $DBI::state');

$drh->set_err(2, "test error 2", "IM999");
is($DBI::state, 'IM999', '... checking $DBI::state');

SKIP: {
	skip "using DBI::PurePerl", 1 if $DBI::PurePerl;
	eval { 
		$DBI::rows = 1 
	};
	like($@, qr/Can't modify/, '... trying to assign to $DBI::rows should throw an excpetion'); #'
}

is($drh->{FetchHashKeyName}, 'NAME', '... FetchHashKeyName is NAME');
$drh->{FetchHashKeyName} = 'NAME_lc';
is($drh->{FetchHashKeyName}, 'NAME_lc', '... FetchHashKeyName is now changed to NAME_lc');

ok(!$drh->disconnect_all, '... calling $drh->disconnect_all (not implemented but will fail silently)');

SKIP: {
	skip "using DBI::PurePerl", 5 if $DBI::PurePerl;
	my $can = $drh->can('FETCH');

	ok($can, '... $drh can FETCH'); 
	is(ref($can), "CODE", '... and it returned a proper CODE ref'); 

	my $name = $can->($drh, "Name");

	ok($name, '... used FETCH returned from can to fetch the Name attribute');
	is($name, "Test", '... the Name attribute is equal to Test');

	ok(!$drh->can('disconnect_all'), '... ');
}

1;
