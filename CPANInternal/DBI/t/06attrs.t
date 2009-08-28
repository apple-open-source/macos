#!perl -w

use strict;

use Test::More tests => 145;

## ----------------------------------------------------------------------------
## 06attrs.t - ...
## ----------------------------------------------------------------------------
# This test checks the parameters and the values associated with them for 
# the three different handles (Driver, Database, Statement)
## ----------------------------------------------------------------------------

BEGIN { 
	use_ok( 'DBI' ) 
}

$|=1;

my $using_autoproxy = ($ENV{DBI_AUTOPROXY});
my $dsn = 'dbi:ExampleP:dummy';

# Connect to the example driver.
my $dbh = DBI->connect($dsn, '', '', { 
    PrintError => 0, RaiseError => 1,
});

isa_ok( $dbh, 'DBI::db' );

# Clean up when we're done.
END { $dbh->disconnect if $dbh };

## ----------------------------------------------------------------------------
# Check the database handle attributes.

#	bit flag attr
ok( $dbh->{Warn},               '... checking Warn attribute for dbh');
ok( $dbh->{Active},             '... checking Active attribute for dbh');
ok( $dbh->{AutoCommit},         '... checking AutoCommit attribute for dbh');
ok(!$dbh->{CompatMode},         '... checking CompatMode attribute for dbh');
ok(!$dbh->{InactiveDestroy},    '... checking InactiveDestory attribute for dbh');
ok(!$dbh->{PrintError},         '... checking PrintError attribute for dbh');
ok( $dbh->{PrintWarn},          '... checking PrintWarn attribute for dbh');	# true because of perl -w above
ok( $dbh->{RaiseError},         '... checking RaiseError attribute for dbh');
ok(!$dbh->{ShowErrorStatement}, '... checking ShowErrorStatement attribute for dbh');
ok(!$dbh->{ChopBlanks},         '... checking ChopBlanks attribute for dbh');
ok(!$dbh->{LongTruncOk},        '... checking LongTrunkOk attribute for dbh');
ok(!$dbh->{TaintIn},            '... checking TaintIn attribute for dbh');
ok(!$dbh->{TaintOut},           '... checking TaintOut attribute for dbh');
ok(!$dbh->{Taint},              '... checking Taint attribute for dbh');
ok(!$dbh->{Executed},           '... checking Executed attribute for dbh');

#	other attr
cmp_ok($dbh->{ErrCount}, '==', 0, '... checking ErrCount attribute for dbh');

SKIP: {
    skip "Kids and ActiveKids attribute not supported under DBI::PurePerl", 2 if $DBI::PurePerl;
    
    cmp_ok($dbh->{Kids},       '==', 0, '... checking Kids attribute for dbh');;
    cmp_ok($dbh->{ActiveKids}, '==', 0, '... checking ActiveKids attribute for dbh');;
}

is($dbh->{CachedKids}, undef,     '... checking CachedKids attribute for dbh');
ok(!defined $dbh->{HandleError},  '... checking HandleError attribute for dbh');
ok(!defined $dbh->{Profile},      '... checking Profile attribute for dbh');
ok(!defined $dbh->{Statement},    '... checking Statement attribute for dbh');
ok(!defined $dbh->{RowCacheSize}, '... checking RowCacheSize attribute for dbh');
ok(!defined $dbh->{ReadOnly},     '... checking ReadOnly attribute for dbh');

is($dbh->{FetchHashKeyName}, 'NAME',  '... checking FetchHashKeyName attribute for dbh');
is($dbh->{Name},             'dummy', '... checking Name attribute for dbh')	# fails for Multiplex
    unless $using_autoproxy && ok(1);

cmp_ok($dbh->{TraceLevel},  '==', $DBI::dbi_debug & 0xF, '... checking TraceLevel attribute for dbh');
cmp_ok($dbh->{LongReadLen}, '==', 80,                    '... checking LongReadLen attribute for dbh');

is_deeply [ $dbh->FETCH_many(qw(HandleError FetchHashKeyName LongReadLen ErrCount)) ],
          [ undef, qw(NAME 80 0) ], 'should be able to FETCH_many';

is $dbh->{examplep_private_dbh_attrib}, 42, 'should see driver-private dbh attribute value';

# Raise an error.
eval { 
    $dbh->do('select foo from foo') 
};
like($@, qr/^DBD::\w+::db do failed: Unknown field names: foo/ , '... catching exception');

ok(defined $dbh->err, '... $dbh->err is undefined');
like($dbh->errstr,  qr/^Unknown field names: foo\b/, '... checking $dbh->errstr');

is($dbh->state, 'S1000', '... checking $dbh->state');

ok($dbh->{Executed}, '... checking Executed attribute for dbh');    # even though it failed
$dbh->{Executed} = 0;       	                            # reset(able)
cmp_ok($dbh->{Executed}, '==', 0, '... checking Executed attribute for dbh (after reset)');

cmp_ok($dbh->{ErrCount}, '==', 1, '... checking ErrCount attribute for dbh (after error was generated)');

## ----------------------------------------------------------------------------
# Test the driver handle attributes.

my $drh = $dbh->{Driver};
isa_ok( $drh, 'DBI::dr' );

ok($dbh->err, '... checking $dbh->err');

cmp_ok($drh->{ErrCount}, '==', 0, '... checking ErrCount attribute for drh');

ok( $drh->{Warn},               '... checking Warn attribute for drh');
ok( $drh->{Active},             '... checking Active attribute for drh');
ok( $drh->{AutoCommit},         '... checking AutoCommit attribute for drh');
ok(!$drh->{CompatMode},         '... checking CompatMode attribute for drh');
ok(!$drh->{InactiveDestroy},    '... checking InactiveDestory attribute for drh');
ok(!$drh->{PrintError},         '... checking PrintError attribute for drh');
ok( $drh->{PrintWarn},          '... checking PrintWarn attribute for drh');	# true because of perl -w above
ok(!$drh->{RaiseError},         '... checking RaiseError attribute for drh');
ok(!$drh->{ShowErrorStatement}, '... checking ShowErrorStatement attribute for drh');
ok(!$drh->{ChopBlanks},         '... checking ChopBlanks attribute for drh');
ok(!$drh->{LongTruncOk},        '... checking LongTrunkOk attribute for drh');
ok(!$drh->{TaintIn},            '... checking TaintIn attribute for drh');
ok(!$drh->{TaintOut},           '... checking TaintOut attribute for drh');
ok(!$drh->{Taint},              '... checking Taint attribute for drh');

SKIP: {
    skip "Executed attribute not supported under DBI::PurePerl", 1 if $DBI::PurePerl;
    
    ok($drh->{Executed}, '... checking Executed attribute for drh') # due to the do() above
}

SKIP: {
    skip "Kids and ActiveKids attribute not supported under DBI::PurePerl", 2 if ($DBI::PurePerl or $dbh->{mx_handle_list});
    cmp_ok($drh->{Kids},       '==', 1, '... checking Kids attribute for drh');
    cmp_ok($drh->{ActiveKids}, '==', 1, '... checking ActiveKids attribute for drh');
}

is($drh->{CachedKids}, undef,    '... checking CachedKids attribute for drh');
ok(!defined $drh->{HandleError}, '... checking HandleError attribute for drh');
ok(!defined $drh->{Profile},     '... checking Profile attribute for drh');
ok(!defined $drh->{ReadOnly},    '... checking ReadOnly attribute for drh');

cmp_ok($drh->{TraceLevel},  '==', $DBI::dbi_debug & 0xF, '... checking TraceLevel attribute for drh');
cmp_ok($drh->{LongReadLen}, '==', 80,                    '... checking LongReadLen attribute for drh');

is($drh->{FetchHashKeyName}, 'NAME',     '... checking FetchHashKeyName attribute for drh');
is($drh->{Name},             'ExampleP', '... checking Name attribute for drh')
    unless $using_autoproxy && ok(1);

## ----------------------------------------------------------------------------
# Test the statement handle attributes.

# Create a statement handle.
my $sth = $dbh->prepare("select ctime, name from ?");
isa_ok($sth, "DBI::st");

ok(!$sth->{Executed}, '... checking Executed attribute for sth');
ok(!$dbh->{Executed}, '... checking Executed attribute for dbh');
cmp_ok($sth->{ErrCount}, '==', 0, '... checking ErrCount attribute for sth');

# Trigger an exception.
eval { 
    $sth->execute("foo") 
};
# we don't check actual opendir error msg because of locale differences
like($@, qr/^DBD::\w+::st execute failed: .*opendir\(foo\): /msi, '... checking exception');

# Test all of the statement handle attributes.
like($sth->errstr, qr/opendir\(foo\): /, '... checking $sth->errstr');
is($sth->state, 'S1000', '... checking $sth->state');
ok($sth->{Executed}, '... checking Executed attribute for sth');	# even though it failed
ok($dbh->{Executed}, '... checking Exceuted attribute for dbh');	# due to $sth->prepare, even though it failed

cmp_ok($sth->{ErrCount}, '==', 1, '... checking ErrCount attribute for sth');
eval { 
    $sth->{ErrCount} = 42 
};
like($@, qr/STORE failed:/, '... checking exception');

cmp_ok($sth->{ErrCount}, '==', 42 , '... checking ErrCount attribute for sth (after assignment)');

$sth->{ErrCount} = 0;
cmp_ok($sth->{ErrCount}, '==', 0, '... checking ErrCount attribute for sth (after reset)');

# booleans
ok( $sth->{Warn},               '... checking Warn attribute for sth');
ok(!$sth->{Active},             '... checking Active attribute for sth');
ok(!$sth->{CompatMode},         '... checking CompatMode attribute for sth');
ok(!$sth->{InactiveDestroy},    '... checking InactiveDestroy attribute for sth');
ok(!$sth->{PrintError},         '... checking PrintError attribute for sth');
ok( $sth->{PrintWarn},          '... checking PrintWarn attribute for sth');
ok( $sth->{RaiseError},         '... checking RaiseError attribute for sth');
ok(!$sth->{ShowErrorStatement}, '... checking ShowErrorStatement attribute for sth');
ok(!$sth->{ChopBlanks},         '... checking ChopBlanks attribute for sth');
ok(!$sth->{LongTruncOk},        '... checking LongTrunkOk attribute for sth');
ok(!$sth->{TaintIn},            '... checking TaintIn attribute for sth');
ok(!$sth->{TaintOut},           '... checking TaintOut attribute for sth');
ok(!$sth->{Taint},              '... checking Taint attribute for sth');

# common attr
SKIP: {
    skip "Kids and ActiveKids attribute not supported under DBI::PurePerl", 2 if $DBI::PurePerl;
    cmp_ok($sth->{Kids},       '==', 0, '... checking Kids attribute for sth');
    cmp_ok($sth->{ActiveKids}, '==', 0, '... checking ActiveKids attribute for sth');
}

ok(!defined $sth->{CachedKids},  '... checking CachedKids attribute for sth');
ok(!defined $sth->{HandleError}, '... checking HandleError attribute for sth');
ok(!defined $sth->{Profile},     '... checking Profile attribute for sth');
ok(!defined $sth->{ReadOnly},    '... checking ReadOnly attribute for sth');

cmp_ok($sth->{TraceLevel},  '==', $DBI::dbi_debug & 0xF, '... checking TraceLevel attribute for sth');
cmp_ok($sth->{LongReadLen}, '==', 80,                    '... checking LongReadLen attribute for sth');

is($sth->{FetchHashKeyName}, 'NAME', '... checking FetchHashKeyName attribute for sth');

# sth specific attr
ok(!defined $sth->{CursorName}, '... checking CursorName attribute for sth');

cmp_ok($sth->{NUM_OF_FIELDS}, '==', 2, '... checking NUM_OF_FIELDS attribute for sth');
cmp_ok($sth->{NUM_OF_PARAMS}, '==', 1, '... checking NUM_OF_PARAMS attribute for sth');

my $name = $sth->{NAME};
is(ref($name), 'ARRAY', '... checking type of NAME attribute for sth');
cmp_ok(scalar(@{$name}), '==', 2, '... checking number of elements returned');
is_deeply($name, ['ctime', 'name' ], '... checking values returned');

my $name_lc = $sth->{NAME_lc};
is(ref($name_lc), 'ARRAY', '... checking type of NAME_lc attribute for sth');
cmp_ok(scalar(@{$name_lc}), '==', 2, '... checking number of elements returned');
is_deeply($name_lc, ['ctime', 'name' ], '... checking values returned');

my $name_uc = $sth->{NAME_uc};
is(ref($name_uc), 'ARRAY', '... checking type of NAME_uc attribute for sth');
cmp_ok(scalar(@{$name_uc}), '==', 2, '... checking number of elements returned');
is_deeply($name_uc, ['CTIME', 'NAME' ], '... checking values returned');

my $nhash = $sth->{NAME_hash};
is(ref($nhash), 'HASH', '... checking type of NAME_hash attribute for sth');
cmp_ok(scalar(keys(%{$nhash})), '==', 2, '... checking number of keys returned');
cmp_ok($nhash->{ctime},         '==', 0, '... checking values returned');
cmp_ok($nhash->{name},          '==', 1, '... checking values returned');

my $nhash_lc = $sth->{NAME_lc_hash};
is(ref($nhash_lc), 'HASH', '... checking type of NAME_lc_hash attribute for sth');
cmp_ok(scalar(keys(%{$nhash_lc})), '==', 2, '... checking number of keys returned');
cmp_ok($nhash_lc->{ctime},         '==', 0, '... checking values returned');
cmp_ok($nhash_lc->{name},          '==', 1, '... checking values returned');

my $nhash_uc = $sth->{NAME_uc_hash};
is(ref($nhash_uc), 'HASH', '... checking type of NAME_uc_hash attribute for sth');
cmp_ok(scalar(keys(%{$nhash_uc})), '==', 2, '... checking number of keys returned');
cmp_ok($nhash_uc->{CTIME},         '==', 0, '... checking values returned');
cmp_ok($nhash_uc->{NAME},          '==', 1, '... checking values returned');

my $type = $sth->{TYPE};
is(ref($type), 'ARRAY', '... checking type of TYPE attribute for sth');
cmp_ok(scalar(@{$type}), '==', 2, '... checking number of elements returned');
is_deeply($type, [ 4, 12 ], '... checking values returned');

my $null = $sth->{NULLABLE};
is(ref($null), 'ARRAY', '... checking type of NULLABLE attribute for sth');
cmp_ok(scalar(@{$null}), '==', 2, '... checking number of elements returned');
is_deeply($null, [ 0, 0 ], '... checking values returned');

# Should these work? They don't.
my $prec = $sth->{PRECISION};
is(ref($prec), 'ARRAY', '... checking type of PRECISION attribute for sth');
cmp_ok(scalar(@{$prec}), '==', 2, '... checking number of elements returned');
is_deeply($prec, [ 10, 1024 ], '... checking values returned');
    
my $scale = $sth->{SCALE};
is(ref($scale), 'ARRAY', '... checking type of SCALE attribute for sth');
cmp_ok(scalar(@{$scale}), '==', 2, '... checking number of elements returned');
is_deeply($scale, [ 0, 0 ], '... checking values returned');

my $params = $sth->{ParamValues};
is(ref($params), 'HASH', '... checking type of ParamValues attribute for sth');
is($params->{1}, 'foo', '... checking values returned');

is($sth->{Statement}, "select ctime, name from ?", '... checking Statement attribute for sth');
ok(!defined $sth->{RowsInCache}, '... checking type of RowsInCache attribute for sth');

is $sth->{examplep_private_sth_attrib}, 24, 'should see driver-private sth attribute value';

# $h->{TraceLevel} tests are in t/09trace.t

print "Checking inheritance\n";

SKIP: {
    skip "drh->dbh->sth inheritance test skipped with DBI_AUTOPROXY", 2 if $ENV{DBI_AUTOPROXY};

sub check_inherited {
    my ($drh, $attr, $value, $skip_sth) = @_;
    local $drh->{$attr} = $value;
    local $drh->{PrintError} = 1;
    my $dbh = $drh->connect("dummy");
    is $dbh->{$attr}, $drh->{$attr}, "dbh $attr value should be inherited from drh";
    unless ($skip_sth) {
        my $sth = $dbh->prepare("select name from .");
        is $sth->{$attr}, $dbh->{$attr}, "sth $attr value should be inherited from dbh";
    }
}

check_inherited($drh, "ReadOnly", 1, 0);

}

1;
# end
