use strict;

BEGIN {
    $|  = 1;
    $^W = 1;
}

use t::lib::Test;
use Test::More tests => 7;
use Test::NoWarnings;
use DBI qw(:sql_types);

my $dbh = connect_ok();
$dbh->do('CREATE TABLE foo (bar TEXT, num INT)');

foreach ( 1..5 ) {
	$dbh->do(
		'INSERT INTO foo (bar, num) VALUES (?, ?)',
		undef, ($_%2 ? "odd" : "even"), $_
	);
}
# DBI->trace(9);

# see if placeholder works
my ($v, $num) = $dbh->selectrow_array(
	'SELECT bar, num FROM foo WHERE num = ?',
	undef, 3
);
ok( $v eq 'odd' && $num == 3 );

# see if the sql itself works as expected
my $ar = $dbh->selectall_arrayref(
	'SELECT bar FROM foo GROUP BY bar HAVING count(*) > 1'
);
is( scalar(@$ar), 2, 'Got 2 results' );

# known workaround 1
# ref: http://code.google.com/p/gears/issues/detail?id=163
$ar = $dbh->selectall_arrayref(
	'SELECT bar FROM foo GROUP BY bar HAVING count(*) > 0+?',
	undef, 1
);
is( scalar(@$ar), 2, 'Got 2 results' );

# known workaround 2
my $sth = $dbh->prepare(
	'SELECT bar FROM foo GROUP BY bar HAVING count(*) > ?',
);
$sth->bind_param(1, 1, { TYPE => SQL_INTEGER });
$sth->execute;
$ar = $sth->fetchall_arrayref;
is( scalar(@$ar), 2, 'Got 2 results' );

# and this is what should be tested
TODO: {
	local $TODO = 'This test is currently broken again. Wait for a better fix, or use known workarounds shown above';
	$ar = $dbh->selectall_arrayref(
		'SELECT bar FROM foo GROUP BY bar HAVING count(*) > ?',
		undef, 1
	);
	# print "4: @$_\n" for @$ar;
	is( scalar(@$ar), 2, "we got ".(@$ar)." items" );
}
