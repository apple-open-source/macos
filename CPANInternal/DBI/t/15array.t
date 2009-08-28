#!perl -w
$|=1;

use strict;

use Test::More tests => 55;

## ----------------------------------------------------------------------------
## 15array.t
## ----------------------------------------------------------------------------
# 
## ----------------------------------------------------------------------------

BEGIN {
	use_ok('DBI');
}

# create a database handle
my $dbh = DBI->connect("dbi:Sponge:dummy", '', '', { 
    RaiseError => 1, 
    ShowErrorStatement => 1,
    AutoCommit => 1 
});

# check that our db handle is good
isa_ok($dbh, "DBI::db");

my $rv;
my $rows         = [];
my $tuple_status = [];
my $dumped;

my $sth = $dbh->prepare("insert", {
		rows          => $rows,   # where to 'insert' (push) the rows
		NUM_OF_PARAMS => 4,
		execute_hook  => sub {    # DBD::Sponge hook to make certain data trigger an error for that row
			local $^W;
			return $_[0]->set_err(1,"errmsg") if grep { $_ and $_ eq "B" } @_;
			return 1;
		}
	});
	
isa_ok($sth, "DBI::st");

cmp_ok(scalar @{$rows}, '==', 0, '... we should have 0 rows');

# -----------------------------------------------

ok(! eval {
        local $sth->{PrintError} = 0;
        $sth->execute_array(
		{
			ArrayTupleStatus => $tuple_status
		},
		[ 1, 2, 3 ],	          # array of integers
		42,                       # scalar 42 treated as array of 42's
		undef,                    # scalar undef treated as array of undef's
		[ qw(A B C) ],	          # array of strings
    ) },
    '... execute_array should return false'
);
ok $@, 'execute_array failure with RaiseError should have died';
like $sth->errstr, '/executing 3 generated 1 errors/';

cmp_ok(scalar @{$rows}, '==', 2, '... we should have 2 rows');
cmp_ok(scalar @{$tuple_status}, '==', 3, '... we should have 3 tuple_status');

ok(eq_array(
		$rows, 
		[ [1, 42, undef, 'A'], [3, 42, undef, 'C'] ]
		),
	'... our rows are as expected');

ok(eq_array(
		$tuple_status,
		[1, [1, 'errmsg', 'S1000'], 1]
		),
	'... our tuple_status is as expected');

# -----------------------------------------------
# --- change one param and re-execute

@$rows = ();
ok( $sth->bind_param_array(4, [ qw(a b c) ]), '... bind_param_array should return true');
ok( $sth->execute_array({ ArrayTupleStatus => $tuple_status }), '... execute_array should return true');

cmp_ok(scalar @{$rows}, '==', 3, '... we should have 3 rows');
cmp_ok(scalar @{$tuple_status}, '==', 3, '... we should have 3 tuple_status');

ok(eq_array(
		$rows, 
		[ [1, 42, undef, 'a'], [2, 42, undef, 'b'], [3, 42, undef, 'c'] ]
		),
	'... our rows are as expected');
		
ok(eq_array(
		$tuple_status,
		[1, 1, 1]
		),
	'... our tuple_status is as expected');

# -----------------------------------------------
# --- call execute_array in array context to get executed AND affected
@$rows = ();
my ($executed, $affected) = $sth->execute_array({ ArrayTupleStatus => $tuple_status });
ok($executed, '... execute_array should return true');
cmp_ok($executed, '==', 3, '... we should have executed 3 rows');
cmp_ok($affected, '==', 3, '... we should have affected 3 rows');

# -----------------------------------------------
# --- with no values for bind params, should execute zero times

@$rows = ();
$rv = $sth->execute_array( { ArrayTupleStatus => $tuple_status }, [], [], [], []);
ok($rv,   '... execute_array should return true');
ok(!($rv+0), '... execute_array should return 0 (but true)');

cmp_ok(scalar @{$rows}, '==', 0, '... we should have 0 rows');
cmp_ok(scalar @{$tuple_status}, '==', 0,'... we should have 0 tuple_status');

# -----------------------------------------------
# --- with only scalar values for bind params, should execute just once

@$rows = ();
$rv = $sth->execute_array( { ArrayTupleStatus => $tuple_status }, 5, 6, 7, 8);
cmp_ok($rv, '==', 1,   '... execute_array should return 1');

cmp_ok(scalar @{$rows}, '==', 1, '... we should have 1 rows');
ok(eq_array( $rows, [ [5,6,7,8] ]), '... our rows are as expected');
cmp_ok(scalar @{$tuple_status}, '==', 1,'... we should have 1 tuple_status');
ok(eq_array( $tuple_status, [1]), '... our tuple_status is as expected');

# -----------------------------------------------
# --- with mix of scalar values and arrays only arrays control tuples

@$rows = ();
$rv = $sth->execute_array( { ArrayTupleStatus => $tuple_status }, 5, [], 7, 8);
cmp_ok($rv, '==', 0,   '... execute_array should return 0');

cmp_ok(scalar @{$rows}, '==', 0, '... we should have 0 rows');
cmp_ok(scalar @{$tuple_status}, '==', 0,'... we should have 0 tuple_status');

# -----------------------------------------------
# --- catch 'undefined value' bug with zero bind values

@$rows = ();
my $sth_other = $dbh->prepare("insert", {
	rows => $rows,		   # where to 'insert' (push) the rows
	NUM_OF_PARAMS => 1,
});

isa_ok($sth_other, "DBI::st");

$rv = $sth_other->execute_array( {}, [] );
ok($rv,   '... execute_array should return true');
ok(!($rv+0), '... execute_array should return 0 (but true)');
# no ArrayTupleStatus

cmp_ok(scalar @{$rows}, '==', 0, '... we should have 0 rows');

# -----------------------------------------------
# --- ArrayTupleFetch code-ref tests ---

my $index = 0;

my $fetchrow = sub {				# generate 5 rows of two integer values
    return if $index >= 2;
    $index +=1;
    # There doesn't seem any reliable way to force $index to be
    # treated as a string (and so dumped as such).  We just have to
    # make the test case allow either 1 or '1'.
    return [ $index, 'a','b','c' ];
};

@$rows = ();
ok( $sth->execute_array({
		ArrayTupleFetch  => $fetchrow,
		ArrayTupleStatus => $tuple_status
	}), '... execute_array should return true');
	
cmp_ok(scalar @{$rows}, '==', 2, '... we should have 2 rows');
cmp_ok(scalar @{$tuple_status}, '==', 2, '... we should have 2 tuple_status');

ok(eq_array(
	$rows, 
	[ [1, 'a', 'b', 'c'], [2, 'a', 'b', 'c'] ]
	),
	'... rows should match'
);

ok(eq_array(
	$tuple_status, 
	[1, 1]
	),
	'... tuple_status should match'
);

# -----------------------------------------------
# --- ArrayTupleFetch sth tests ---

my $fetch_sth = $dbh->prepare("foo", {
        rows          => [ map { [ $_,'x','y','z' ] } 7..9 ],
        NUM_OF_FIELDS => 4
	});
	
isa_ok($fetch_sth, "DBI::st");	

$fetch_sth->execute();

@$rows = ();

ok( $sth->execute_array({
		ArrayTupleFetch  => $fetch_sth,
		ArrayTupleStatus => $tuple_status,
	}), '... execute_array should return true');

cmp_ok(scalar @{$rows}, '==', 3, '... we should have 3 rows');
cmp_ok(scalar @{$tuple_status}, '==', 3, '... we should have 3 tuple_status');

ok(eq_array(
	$rows, 
	[ [7, 'x', 'y', 'z'], [8, 'x', 'y', 'z'], [9, 'x', 'y', 'z'] ]
	),
	'... rows should match'
);

ok(eq_array(
	$tuple_status, 
	[1, 1, 1]
	), 
	'... tuple status should match'
);

# -----------------------------------------------
# --- error detection tests ---

$sth->{RaiseError} = 0;
$sth->{PrintError} = 0;

ok(!defined $sth->execute_array( { ArrayTupleStatus => $tuple_status }, [1],[2]), '... execute_array should return undef');
is($sth->errstr, '2 bind values supplied but 4 expected', '... errstr is as expected');

ok(!defined $sth->execute_array( { ArrayTupleStatus => { } }, [ 1, 2, 3 ]), '... execute_array should return undef');
is( $sth->errstr, 'ArrayTupleStatus attribute must be an arrayref', '... errstr is as expected');

ok(!defined $sth->execute_array( { ArrayTupleStatus => $tuple_status }, 1,{},3,4), '... execute_array should return undef');
is( $sth->errstr, 'Value for parameter 2 must be a scalar or an arrayref, not a HASH', '... errstr is as expected');

ok(!defined $sth->bind_param_array(":foo", [ qw(a b c) ]), '... bind_param_array should return undef');
is( $sth->errstr, "Can't use named placeholder ':foo' for non-driver supported bind_param_array", '... errstr is as expected');

$dbh->disconnect;

1;
