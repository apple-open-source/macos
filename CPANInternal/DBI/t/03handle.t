#!perl -w
$|=1;

use strict;

use Test::More tests => 137;

## ----------------------------------------------------------------------------
## 03handle.t - tests handles
## ----------------------------------------------------------------------------
# This set of tests exercises the different handles; Driver, Database and 
# Statement in various ways, in particular in their interactions with one
# another
## ----------------------------------------------------------------------------

BEGIN { 
    use_ok( 'DBI' );
}

# installed drivers should start empty
my %drivers = DBI->installed_drivers();
is(scalar keys %drivers, 0);

## ----------------------------------------------------------------------------
# get the Driver handle

my $driver = "ExampleP";

my $drh = DBI->install_driver($driver);
isa_ok( $drh, 'DBI::dr' );

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh->{Kids}, '==', 0, '... this Driver does not yet have any Kids');
}

# now the driver should be registered
%drivers = DBI->installed_drivers();
is(scalar keys %drivers, 1);
ok(exists $drivers{ExampleP});
ok($drivers{ExampleP}->isa('DBI::dr'));

my $using_dbd_gofer = ($ENV{DBI_AUTOPROXY}||'') =~ /^dbi:Gofer.*transport=/i;

## ----------------------------------------------------------------------------
# do database handle tests inside do BLOCK to capture scope

do {
    my $dbh = DBI->connect("dbi:$driver:", '', '');
    isa_ok($dbh, 'DBI::db');

    my $drh = $dbh->{Driver}; # (re)get drh here so tests can work using_dbd_gofer
    
    SKIP: {
        skip "Kids and ActiveKids attributes not supported under DBI::PurePerl", 2 if $DBI::PurePerl;
    
        cmp_ok($drh->{Kids}, '==', 1, '... our Driver has one Kid');
        cmp_ok($drh->{ActiveKids}, '==', 1, '... our Driver has one ActiveKid');  
    }

    my $sql = "select name from ?";

    my $sth1 = $dbh->prepare_cached($sql);
    isa_ok($sth1, 'DBI::st');    
    ok($sth1->execute("."), '... execute ran successfully');

    my $ck = $dbh->{CachedKids};
    is(ref($ck), "HASH", '... we got the CachedKids hash');
    
    cmp_ok(scalar(keys(%{$ck})), '==', 1, '... there is one CachedKid');
    ok(eq_set(
        [ values %{$ck} ],
        [ $sth1 ]
        ), 
    '... our statment handle should be in the CachedKids');

    ok($sth1->{Active}, '... our first statment is Active');
    
    {
	my $warn = 0; # use this to check that we are warned
	local $SIG{__WARN__} = sub { ++$warn if $_[0] =~ /still active/i };
	
	my $sth2 = $dbh->prepare_cached($sql);
	isa_ok($sth2, 'DBI::st');
	
	is($sth1, $sth2, '... prepare_cached returned the same statement handle');
	cmp_ok($warn,'==', 1, '... we got warned about our first statement handle being still active');
	
	ok(!$sth1->{Active}, '... our first statment is no longer Active since we re-prepared it');

	my $sth3 = $dbh->prepare_cached($sql, { foo => 1 });
	isa_ok($sth3, 'DBI::st');
	
	isnt($sth1, $sth3, '... prepare_cached returned a different statement handle now');
	cmp_ok(scalar(keys(%{$ck})), '==', 2, '... there are two CachedKids');
	ok(eq_set(
	    [ values %{$ck} ],
	    [ $sth1, $sth3 ]
	    ), 
	'... both statment handles should be in the CachedKids');    

	ok($sth1->execute("."), '... executing first statement handle again');
	ok($sth1->{Active}, '... first statement handle is now active again');
	
	my $sth4 = $dbh->prepare_cached($sql, undef, 3);
	isa_ok($sth4, 'DBI::st');
	
	isnt($sth1, $sth4, '... our fourth statement handle is not the same as our first');
	ok($sth1->{Active}, '... first statement handle is still active');
	
	cmp_ok(scalar(keys(%{$ck})), '==', 2, '... there are two CachedKids');    
	ok(eq_set(
	    [ values %{$ck} ],
	    [ $sth2, $sth4 ]
	    ), 
	'... second and fourth statment handles should be in the CachedKids');      
	
	$sth1->finish;
	ok(!$sth1->{Active}, '... first statement handle is no longer active');    

	ok($sth4->execute("."), '... fourth statement handle executed properly');
	ok($sth4->{Active}, '... fourth statement handle is Active');

	my $sth5 = $dbh->prepare_cached($sql, undef, 1);
	isa_ok($sth5, 'DBI::st');
	
	cmp_ok($warn, '==', 1, '... we still only got one warning');

	is($sth4, $sth5, '... fourth statement handle and fifth one match');
	ok(!$sth4->{Active}, '... fourth statement handle is not Active');
	ok(!$sth5->{Active}, '... fifth statement handle is not Active (shouldnt be its the same as fifth)');
	
	cmp_ok(scalar(keys(%{$ck})), '==', 2, '... there are two CachedKids');    
	ok(eq_set(
	    [ values %{$ck} ],
	    [ $sth2, $sth5 ]
	    ), 
	'... second and fourth/fifth statment handles should be in the CachedKids');     
    }

    SKIP: {
	skip "swap_inner_handle() not supported under DBI::PurePerl", 23 if $DBI::PurePerl;
    
        my $sth6 = $dbh->prepare($sql);
        $sth6->execute(".");
        my $sth1_driver_name = $sth1->{Database}{Driver}{Name};

        ok( $sth6->{Active}, '... sixth statement handle is active');
        ok(!$sth1->{Active}, '... first statement handle is not active');

        ok($sth1->swap_inner_handle($sth6), '... first statement handle becomes the sixth');
        ok(!$sth6->{Active}, '... sixth statement handle is now not active');
        ok( $sth1->{Active}, '... first statement handle is now active again');

        ok($sth1->swap_inner_handle($sth6), '... first statement handle becomes the sixth');
        ok( $sth6->{Active}, '... sixth statement handle is active');
        ok(!$sth1->{Active}, '... first statement handle is not active');

        ok($sth1->swap_inner_handle($sth6), '... first statement handle becomes the sixth');
        ok(!$sth6->{Active}, '... sixth statement handle is now not active');
        ok( $sth1->{Active}, '... first statement handle is now active again');

	$sth1->{PrintError} = 0;
        ok(!$sth1->swap_inner_handle($dbh), '... can not swap a sth with a dbh');
	cmp_ok( $sth1->errstr, 'eq', "Can't swap_inner_handle between sth and dbh");

        ok($sth1->swap_inner_handle($sth6), '... first statement handle becomes the sixth');
        ok( $sth6->{Active}, '... sixth statement handle is active');
        ok(!$sth1->{Active}, '... first statement handle is not active');

        $sth6->finish;

	ok(my $dbh_nullp = DBI->connect("dbi:NullP:", undef, undef, { go_bypass => 1 }));
	ok(my $sth7 = $dbh_nullp->prepare(""));

	$sth1->{PrintError} = 0;
        ok(!$sth1->swap_inner_handle($sth7), "... can't swap_inner_handle with handle from different parent");
	cmp_ok( $sth1->errstr, 'eq', "Can't swap_inner_handle with handle from different parent");

	cmp_ok( $sth1->{Database}{Driver}{Name}, 'eq', $sth1_driver_name );
        ok( $sth1->swap_inner_handle($sth7,1), "... can swap to different parent if forced");
	cmp_ok( $sth1->{Database}{Driver}{Name}, 'eq', "NullP" );

	$dbh_nullp->disconnect;
    }

    ok(  $dbh->ping, 'ping should be true before disconnect');
    $dbh->disconnect;
    $dbh->{PrintError} = 0; # silence 'not connected' warning
    ok( !$dbh->ping, 'ping should be false after disconnect');

    SKIP: {
        skip "Kids and ActiveKids attributes not supported under DBI::PurePerl", 2 if $DBI::PurePerl;
    
        cmp_ok($drh->{Kids}, '==', 1, '... our Driver has one Kid after disconnect');
        cmp_ok($drh->{ActiveKids}, '==', 0, '... our Driver has no ActiveKids after disconnect');      
    }
    
};

if ($using_dbd_gofer) {
    $drh->{CachedKids} = {};
}

# make sure our driver has no more kids after this test
# NOTE:
# this also assures us that the next test has an empty slate as well
SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh->{Kids}, '==', 0, "... our $drh->{Name} driver should have 0 Kids after dbh was destoryed");
}

## ----------------------------------------------------------------------------
# handle reference leak tests

# NOTE: 
# this test checks for reference leaks by testing the Kids attribute
# which is not supported by DBI::PurePerl, so we just do not run this
# for DBI::PurePerl all together. Even though some of the tests would
# pass, it does not make sense becuase in the end, what is actually
# being tested for will give a false positive

sub work {
    my (%args) = @_;
    my $dbh = DBI->connect("dbi:$driver:", '', '');
    isa_ok( $dbh, 'DBI::db' );
    
    cmp_ok($drh->{Kids}, '==', 1, '... the Driver should have 1 Kid(s) now'); 
    
    if ( $args{Driver} ) {
        isa_ok( $dbh->{Driver}, 'DBI::dr' );
    } else {
        pass( "not testing Driver here" );
    }

    my $sth = $dbh->prepare_cached("select name from ?");
    isa_ok( $sth, 'DBI::st' );
    
    if ( $args{Database} ) {
        isa_ok( $sth->{Database}, 'DBI::db' );
    } else {
        pass( "not testing Database here" );
    }
    
    $dbh->disconnect;
    # both handles should be freed here
}

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 25 if $DBI::PurePerl;
    skip "drh Kids not testable under DBD::Gofer", 25 if $using_dbd_gofer;

    foreach my $args (
        {},
        { Driver   => 1 },
        { Database => 1 },
        { Driver   => 1, Database => 1 },
    ) {
        work( %{$args} );
        cmp_ok($drh->{Kids}, '==', 0, '... the Driver should have no Kids');
    }

    # make sure we have no kids when we end this
    cmp_ok($drh->{Kids}, '==', 0, '... the Driver should have no Kids at the end of this test');
}

## ----------------------------------------------------------------------------
# handle take_imp_data test

SKIP: {
    skip "take_imp_data test not supported under DBD::Gofer", 19 if $using_dbd_gofer;

    my $dbh = DBI->connect("dbi:$driver:", '', '');
    isa_ok($dbh, "DBI::db");
    my $drh = $dbh->{Driver}; # (re)get drh here so tests can work using_dbd_gofer

    cmp_ok($drh->{Kids}, '==', 1, '... our Driver should have 1 Kid(s) here')
        unless $DBI::PurePerl && pass();

    $dbh->prepare("select name from ?"); # destroyed at once
    my $sth2 = $dbh->prepare("select name from ?"); # inactive
    my $sth3 = $dbh->prepare("select name from ?"); # active:
    $sth3->execute(".");
    is $sth3->{Active}, 1;
    is $dbh->{ActiveKids}, 1
        unless $DBI::PurePerl && pass();

    my $ChildHandles = $dbh->{ChildHandles};

    skip "take_imp_data test needs weakrefs", 15 if not $ChildHandles;

    ok $ChildHandles, 'we need weakrefs for take_imp_data to work safely with child handles';
    is @$ChildHandles, 3, 'should have 3 entries (implementation detail)';
    is grep({ defined } @$ChildHandles), 2, 'should have 2 defined handles';

    my $imp_data = $dbh->take_imp_data;
    ok($imp_data, '... we got some imp_data to test');
    # generally length($imp_data) = 112 for 32bit, 116 for 64 bit
    # (as of DBI 1.37) but it can differ on some platforms
    # depending on structure packing by the compiler
    # so we just test that it's something reasonable:
    cmp_ok(length($imp_data), '>=', 80, '... test that our imp_data is greater than or equal to 80, this is reasonable');

    cmp_ok($drh->{Kids}, '==', 0, '... our Driver should have 0 Kid(s) after calling take_imp_data');

    is ref $sth3, 'DBI::zombie', 'sth should be reblessed';
    eval { $sth3->finish };
    like $@, qr/Can't locate object method/;

    {
        my @warn;
        local $SIG{__WARN__} = sub { push @warn, $_[0] if $_[0] =~ /after take_imp_data/; print "warn: @_\n"; };
        
        my $drh = $dbh->{Driver};
        ok(!defined $drh, '... our Driver should be undefined');
        
        my $trace_level = $dbh->{TraceLevel};
        ok(!defined $trace_level, '... our TraceLevel should be undefined');

        ok(!defined $dbh->disconnect, '... disconnect should return undef');

        ok(!defined $dbh->quote(42), '... quote should return undefined');

        cmp_ok(scalar @warn, '==', 4, '... we should have gotten 4 warnings');
    }

    my $dbh2 = DBI->connect("dbi:$driver:", '', '', { dbi_imp_data => $imp_data });
    isa_ok($dbh2, "DBI::db");
    # need a way to test dbi_imp_data has been used
    
    cmp_ok($drh->{Kids}, '==', 1, '... our Driver should have 1 Kid(s) again')
        unless $DBI::PurePerl && pass();
    
}

# we need this SKIP block on its own since we are testing the 
# destruction of objects within the scope of the above SKIP 
# block
SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh->{Kids}, '==', 0, '... our Driver has no Kids after this test');
}

## ----------------------------------------------------------------------------
# NullP statement handle attributes without execute

my $driver2 = "NullP";

my $drh2 = DBI->install_driver($driver);
isa_ok( $drh2, 'DBI::dr' );

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh2->{Kids}, '==', 0, '... our Driver (2) has no Kids before this test');
}

do {
    my $dbh = DBI->connect("dbi:$driver2:", '', '');
    isa_ok($dbh, "DBI::db");

    my $sth = $dbh->prepare("foo bar");
    isa_ok($sth, "DBI::st");

    cmp_ok($sth->{NUM_OF_PARAMS}, '==', 0, '... NUM_OF_PARAMS is 0');
    is($sth->{NUM_OF_FIELDS}, undef, '... NUM_OF_FIELDS should be undef');
    is($sth->{Statement}, "foo bar", '... Statement is "foo bar"');

    ok(!defined $sth->{NAME},         '... NAME is undefined');
    ok(!defined $sth->{TYPE},         '... TYPE is undefined');
    ok(!defined $sth->{SCALE},        '... SCALE is undefined');
    ok(!defined $sth->{PRECISION},    '... PRECISION is undefined');
    ok(!defined $sth->{NULLABLE},     '... NULLABLE is undefined');
    ok(!defined $sth->{RowsInCache},  '... RowsInCache is undefined');
    ok(!defined $sth->{ParamValues},  '... ParamValues is undefined');
    # derived NAME attributes
    ok(!defined $sth->{NAME_uc},      '... NAME_uc is undefined');
    ok(!defined $sth->{NAME_lc},      '... NAME_lc is undefined');
    ok(!defined $sth->{NAME_hash},    '... NAME_hash is undefined');
    ok(!defined $sth->{NAME_uc_hash}, '... NAME_uc_hash is undefined');
    ok(!defined $sth->{NAME_lc_hash}, '... NAME_lc_hash is undefined');

    my $dbh_ref = ref($dbh);
    my $sth_ref = ref($sth);

    ok($dbh_ref->can("prepare"), '... $dbh can call "prepare"');
    ok(!$dbh_ref->can("nonesuch"), '... $dbh cannot call "nonesuch"');
    ok($sth_ref->can("execute"), '... $sth can call "execute"');

    # what is this test for??

    # I don't know why this warning has the "(perhaps ...)" suffix, it shouldn't:
    # Can't locate object method "nonesuch" via package "DBI::db" (perhaps you forgot to load "DBI::db"?)
    eval { ref($dbh)->nonesuch; };

    $dbh->disconnect;
};

SKIP: {
    skip "Kids attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    cmp_ok($drh2->{Kids}, '==', 0, '... our Driver (2) has no Kids after this test');
}

## ----------------------------------------------------------------------------

1;
