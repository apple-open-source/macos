#!perl -w

use strict;

use Test::More tests => 130;
use File::Spec;

$|=1;

## ----------------------------------------------------------------------------
## 01basic.t - test of some basic DBI functions
## ----------------------------------------------------------------------------
# Mostly this script takes care of testing the items exported by the 3
# tags below (in this order):
#		- :sql_types
#		- :squl_cursor_types
#		- :util
# It also then handles some other class methods and functions of DBI, such
# as the following:
#		- $DBI::dbi_debug & its relation to DBI->trace
#		- DBI->internal
#			and then tests on that return value:
#			- $i->debug
#			- $i->{DebugDispatch}
#			- $i->{Warn}
#			- $i->{Attribution}
#			- $i->{Version}
#			- $i->{private_test1}
#			- $i->{cachedKids}
#			- $i->{Kids}
#			- $i->{ActiveKids}
#			- $i->{Active}
#			- and finally that it will not autovivify
#		- DBI->available_drivers
#		- DBI->installed_versions (only for developers)
## ----------------------------------------------------------------------------

## load DBI and export some symbols
BEGIN {
	use_ok('DBI', qw(
					:sql_types
					:sql_cursor_types
					:utils
					));
}

## ----------------------------------------------------------------------------
## testing the :sql_types exports

cmp_ok(SQL_GUID                          , '==', -11, '... testing sql_type');
cmp_ok(SQL_WLONGVARCHAR					 , '==', -10, '... testing sql_type');
cmp_ok(SQL_WVARCHAR                      , '==', -9,  '... testing sql_type');
cmp_ok(SQL_WCHAR                         , '==', -8,  '... testing sql_type');
cmp_ok(SQL_BIT                           , '==', -7,  '... testing sql_type');
cmp_ok(SQL_TINYINT                       , '==', -6,  '... testing sql_type');
cmp_ok(SQL_BIGINT                        , '==', -5,  '... testing sql_type');
cmp_ok(SQL_LONGVARBINARY                 , '==', -4,  '... testing sql_type');
cmp_ok(SQL_VARBINARY                     , '==', -3,  '... testing sql_type');
cmp_ok(SQL_BINARY                        , '==', -2,  '... testing sql_type');
cmp_ok(SQL_LONGVARCHAR                   , '==', -1,  '... testing sql_type');
cmp_ok(SQL_UNKNOWN_TYPE					 , '==', 0,   '... testing sql_type');
cmp_ok(SQL_ALL_TYPES					 , '==', 0,   '... testing sql_type');
cmp_ok(SQL_CHAR							 , '==', 1,   '... testing sql_type');
cmp_ok(SQL_NUMERIC						 , '==', 2,   '... testing sql_type');
cmp_ok(SQL_DECIMAL						 , '==', 3,   '... testing sql_type');
cmp_ok(SQL_INTEGER						 , '==', 4,   '... testing sql_type');
cmp_ok(SQL_SMALLINT						 , '==', 5,   '... testing sql_type');
cmp_ok(SQL_FLOAT						 , '==', 6,   '... testing sql_type');
cmp_ok(SQL_REAL							 , '==', 7,   '... testing sql_type');
cmp_ok(SQL_DOUBLE						 , '==', 8,   '... testing sql_type');
cmp_ok(SQL_DATETIME						 , '==', 9,   '... testing sql_type');
cmp_ok(SQL_DATE							 , '==', 9,   '... testing sql_type');
cmp_ok(SQL_INTERVAL						 , '==', 10,  '... testing sql_type');
cmp_ok(SQL_TIME							 , '==', 10,  '... testing sql_type');
cmp_ok(SQL_TIMESTAMP					 , '==', 11,  '... testing sql_type');
cmp_ok(SQL_VARCHAR						 , '==', 12,  '... testing sql_type');
cmp_ok(SQL_BOOLEAN						 , '==', 16,  '... testing sql_type');
cmp_ok(SQL_UDT							 , '==', 17,  '... testing sql_type');
cmp_ok(SQL_UDT_LOCATOR					 , '==', 18,  '... testing sql_type');
cmp_ok(SQL_ROW							 , '==', 19,  '... testing sql_type');
cmp_ok(SQL_REF							 , '==', 20,  '... testing sql_type');
cmp_ok(SQL_BLOB							 , '==', 30,  '... testing sql_type');
cmp_ok(SQL_BLOB_LOCATOR					 , '==', 31,  '... testing sql_type');
cmp_ok(SQL_CLOB							 , '==', 40,  '... testing sql_type');
cmp_ok(SQL_CLOB_LOCATOR					 , '==', 41,  '... testing sql_type');
cmp_ok(SQL_ARRAY						 , '==', 50,  '... testing sql_type');
cmp_ok(SQL_ARRAY_LOCATOR				 , '==', 51,  '... testing sql_type');
cmp_ok(SQL_MULTISET						 , '==', 55,  '... testing sql_type');
cmp_ok(SQL_MULTISET_LOCATOR				 , '==', 56,  '... testing sql_type');
cmp_ok(SQL_TYPE_DATE					 , '==', 91,  '... testing sql_type');
cmp_ok(SQL_TYPE_TIME					 , '==', 92,  '... testing sql_type');
cmp_ok(SQL_TYPE_TIMESTAMP				 , '==', 93,  '... testing sql_type');
cmp_ok(SQL_TYPE_TIME_WITH_TIMEZONE		 , '==', 94,  '... testing sql_type');
cmp_ok(SQL_TYPE_TIMESTAMP_WITH_TIMEZONE  , '==', 95,  '... testing sql_type');
cmp_ok(SQL_INTERVAL_YEAR                 , '==', 101, '... testing sql_type');
cmp_ok(SQL_INTERVAL_MONTH                , '==', 102, '... testing sql_type');
cmp_ok(SQL_INTERVAL_DAY                  , '==', 103, '... testing sql_type');
cmp_ok(SQL_INTERVAL_HOUR                 , '==', 104, '... testing sql_type');
cmp_ok(SQL_INTERVAL_MINUTE               , '==', 105, '... testing sql_type');
cmp_ok(SQL_INTERVAL_SECOND               , '==', 106, '... testing sql_type');
cmp_ok(SQL_INTERVAL_YEAR_TO_MONTH        , '==', 107, '... testing sql_type');
cmp_ok(SQL_INTERVAL_DAY_TO_HOUR          , '==', 108, '... testing sql_type');
cmp_ok(SQL_INTERVAL_DAY_TO_MINUTE        , '==', 109, '... testing sql_type');
cmp_ok(SQL_INTERVAL_DAY_TO_SECOND        , '==', 110, '... testing sql_type');
cmp_ok(SQL_INTERVAL_HOUR_TO_MINUTE       , '==', 111, '... testing sql_type');
cmp_ok(SQL_INTERVAL_HOUR_TO_SECOND       , '==', 112, '... testing sql_type');
cmp_ok(SQL_INTERVAL_MINUTE_TO_SECOND     , '==', 113, '... testing sql_type');

## ----------------------------------------------------------------------------
## testing the :sql_cursor_types exports

cmp_ok(SQL_CURSOR_FORWARD_ONLY,  '==', 0, '... testing sql_cursor_types');
cmp_ok(SQL_CURSOR_KEYSET_DRIVEN, '==', 1, '... testing sql_cursor_types');
cmp_ok(SQL_CURSOR_DYNAMIC,       '==', 2, '... testing sql_cursor_types');
cmp_ok(SQL_CURSOR_STATIC,        '==', 3, '... testing sql_cursor_types');
cmp_ok(SQL_CURSOR_TYPE_DEFAULT,  '==', 0, '... testing sql_cursor_types');

## ----------------------------------------------------------------------------
## test the :util exports

## testing looks_like_number

my @is_num = looks_like_number(undef, "", "foo", 1, ".", 2, "2");

ok(!defined $is_num[0], '... looks_like_number : undef -> undef');
ok(!defined $is_num[1], '... looks_like_number : "" -> undef (eg "don\'t know")');
ok( defined $is_num[2], '... looks_like_number : "foo" -> defined false');
ok(        !$is_num[2], '... looks_like_number : "foo" -> defined false');
ok(		    $is_num[3], '... looks_like_number : 1 -> true');
ok(		   !$is_num[4], '... looks_like_number : "." -> false');
ok(			$is_num[5], '... looks_like_number : 1 -> true');
ok(			$is_num[6], '... looks_like_number : 1 -> true');

## testing neat

cmp_ok($DBI::neat_maxlen, '==',  400, "... $DBI::neat_maxlen initial state is 400");

is(neat(1 + 1), "2",	 '... neat : 1 + 1 -> "2"');
is(neat("2"),   "'2'",   '... neat : 2 -> "\'2\'"');
is(neat(undef), "undef", '... neat : undef -> "undef"');

## testing neat_list

is(neat_list([ 1 + 1, "2", undef, "foobarbaz"], 8, "|"), "2|'2'|undef|'foo...'", '... test array argument w/seperator and maxlen');
is(neat_list([ 1 + 1, "2", undef, "foobarbaz"]), "2, '2', undef, 'foobarbaz'", '... test array argument w/out seperator or maxlen');


## ----------------------------------------------------------------------------
## testing DBI functions

## test DBI->internal

my $switch = DBI->internal;

isa_ok($switch, 'DBI::dr');

## checking attributes of $switch

# NOTE:
# check too see if this covers all the attributes or not

# TO DO:
# these three can be improved
$switch->debug(0);
pass('... test debug');
$switch->{DebugDispatch} = 0;	# handled by Switch
pass('... test DebugDispatch');
$switch->{Warn} = 1;			# handled by DBI core
pass('... test Warn');

like($switch->{'Attribution'}, qr/DBI.*? by Tim Bunce/, '... this should say Tim Bunce');

# is this being presumptious?
is($switch->{'Version'}, $DBI::VERSION, '... the version should match DBI version');

cmp_ok(($switch->{private_test1} = 1), '==', 1, '... this should work and return 1');
cmp_ok($switch->{private_test1},       '==', 1, '... this should equal 1');

is($switch->{CachedKids}, undef, '... CachedKids should be undef initially');
my $cache = {};
$switch->{CachedKids} = $cache;
is($switch->{CachedKids}, $cache,      '... CachedKids should be our ref');

cmp_ok($switch->{Kids},       '==', 0, '... this should be zero');
cmp_ok($switch->{ActiveKids}, '==', 0, '... this should be zero');

ok($switch->{Active}, '... Active flag is true');

# test attribute warnings
{
	my $warn = "";
	local $SIG{__WARN__} = sub { $warn .= "@_" };
	$switch->{FooBarUnknown} = 1;
	like($warn, qr/Can't set.*FooBarUnknown/, '... we should get a warning here');

	$warn = "";
	$_ = $switch->{BarFooUnknown};
	like($warn, qr/Can't get.*BarFooUnknown/, '... we should get a warning here');

	$warn = "";
	my $dummy = $switch->{$_} for qw(private_foo dbd_foo dbi_foo); # special cases
	cmp_ok($warn, 'eq', "", '... we should get no warnings here');
}

# is this here for a reason? Are we testing anything?

$switch->trace_msg("Test \$h->trace_msg text.\n", 1);
DBI->trace_msg("Test DBI->trace_msg text.\n", 1);

## testing DBI->available_drivers

my @drivers = DBI->available_drivers();
cmp_ok(scalar(@drivers), '>', 0, '... we at least have one driver installed');

# NOTE:
# we lowercase the interpolated @drivers array
# so that our reg-exp will match on VMS & Win32

like(lc("@drivers"), qr/examplep/, '... we should at least have ExampleP installed');

# call available_drivers in scalar context

my $num_drivers = DBI->available_drivers;
cmp_ok($num_drivers, '>', 0, '... we should at least have one driver');

## testing DBI::hash

cmp_ok(DBI::hash("foo1"  ), '==', -1077531989, '... should be -1077531989');
cmp_ok(DBI::hash("foo1",0), '==', -1077531989, '... should be -1077531989');
cmp_ok(DBI::hash("foo2",0), '==', -1077531990, '... should be -1077531990');
SKIP: {
    skip("Math::BigInt < 1.56",2)
	if $DBI::PurePerl && !eval { require Math::BigInt; require_version Math::BigInt 1.56 };
    skip("Math::BigInt $Math::BigInt::VERSION broken",2)
	if $DBI::PurePerl && $Math::BigInt::VERSION =~ /^1\.8[45]/;
    my $bigint_vers = $Math::BigInt::VERSION || "";
    if (!$DBI::PurePerl) {
        cmp_ok(DBI::hash("foo1",1), '==', -1263462440);
        cmp_ok(DBI::hash("foo2",1), '==', -1263462437);
    }
    else {
        # for PurePerl we use Math::BigInt but that's often caused test failures that
        # aren't DBI's fault. So we just warn (via a skip) if it's not working right.
        skip("Seems like your Math::BigInt $Math::BigInt::VERSION has a bug",2)
            unless (DBI::hash("foo1X",1) == -1263462440) && (DBI::hash("foo2",1) == -1263462437);
        ok(1, "Math::BigInt $Math::BigInt::VERSION worked okay");
        ok(1);
    }
}

is(data_string_desc(""), "UTF8 off, ASCII, 0 characters 0 bytes");
is(data_string_desc(42), "UTF8 off, ASCII, 2 characters 2 bytes");
is(data_string_desc("foo"), "UTF8 off, ASCII, 3 characters 3 bytes");
is(data_string_desc(undef), "UTF8 off, undef");
is(data_string_desc("bar\x{263a}"), "UTF8 on, non-ASCII, 4 characters 6 bytes");
is(data_string_desc("\xEA"), "UTF8 off, non-ASCII, 1 characters 1 bytes");

is(data_string_diff(   "",   ""), "");
is(data_string_diff(   "",undef), "String b is undef, string a has 0 characters");
is(data_string_diff(undef,undef), "");
is(data_string_diff("aaa","aaa"), "");

is(data_string_diff("aaa","aba"), "Strings differ at index 1: a[1]=a, b[1]=b");
is(data_string_diff("aba","aaa"), "Strings differ at index 1: a[1]=b, b[1]=a");
is(data_string_diff("aa" ,"aaa"), "String a truncated after 2 characters");
is(data_string_diff("aaa","aa" ), "String b truncated after 2 characters");

is(data_diff(   "",   ""), "");
is(data_diff(undef,undef), "");
is(data_diff("aaa","aaa"), "");

is(data_diff(   "",undef),
	join "","a: UTF8 off, ASCII, 0 characters 0 bytes\n",
		"b: UTF8 off, undef\n",
		"String b is undef, string a has 0 characters\n");
is(data_diff("aaa","aba"),
	join "","a: UTF8 off, ASCII, 3 characters 3 bytes\n",
		"b: UTF8 off, ASCII, 3 characters 3 bytes\n",
		"Strings differ at index 1: a[1]=a, b[1]=b\n");
is(data_diff(pack("C",0xEA), pack("U",0xEA)),
	join "", "a: UTF8 off, non-ASCII, 1 characters 1 bytes\n",
		 "b: UTF8 on, non-ASCII, 1 characters 2 bytes\n",
		 "Strings contain the same sequence of characters\n");
is(data_diff(pack("C",0xEA), pack("U",0xEA), 1), ""); # no logical difference


## ----------------------------------------------------------------------------
# restrict this test to just developers

SKIP: {
	skip 'developer tests', 4 unless -d ".svn";

	print "Test DBI->installed_versions (for @drivers)\n";
	print "(If one of those drivers, or the configuration for it, is bad\n";
	print "then these tests can kill or freeze the process here. That's not the DBI's fault.)\n";
	$SIG{ALRM} = sub {
		die "Test aborted because a driver (one of: @drivers) hung while loading"
		   ." (almost certainly NOT a DBI problem)";
	};
	alarm(20);

	## ----------------------------------------------------------------------------
	## test installed_versions

	# scalar context
	my $installed_versions = DBI->installed_versions;

	is(ref($installed_versions), 'HASH', '... we got a hash of installed versions');
	cmp_ok(scalar(keys(%{$installed_versions})), '>=', 1, '... make sure we have at least one');

	# list context
	my @installed_drivers = DBI->installed_versions;

	cmp_ok(scalar(@installed_drivers), '>=', 1, '... make sure we got at least one');
	like("@installed_drivers", qr/Sponge/, '... make sure at least one of them is DBI::Spounge');
}

## testing dbi_debug

cmp_ok($DBI::dbi_debug, '==',  0, "... DBI::dbi_debug's initial state is 0");

SKIP: {
    my $null = File::Spec->devnull();
    skip "cannot find : $null", 2 unless ($^O eq "MSWin32" || -e $null);

    DBI->trace(15,$null);
    cmp_ok($DBI::dbi_debug, '==', 15, "... DBI::dbi_debug is 15");
    DBI->trace(0, undef);
    cmp_ok($DBI::dbi_debug, '==',  0, "... DBI::dbi_debug is 0");
}

1;
