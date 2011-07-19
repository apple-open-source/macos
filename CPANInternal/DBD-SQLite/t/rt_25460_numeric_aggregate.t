#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 14;
use Test::NoWarnings;

# Create the table
my $dbh = connect_ok();
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE' );
create table foo (
	id integer primary key not null,
	mygroup varchar(255) not null,
	mynumber numeric(20,3) not null
)
END_SQL

# Fill the table
my @data = qw{
	a -2
	a 1
	b 2
	b 1
	c 3
	c -1
	d 4
	d 5
	e 6
	e 7
};
$dbh->begin_work;
while ( @data ) {
	ok $dbh->do(
		'insert into foo ( mygroup, mynumber ) values ( ?, ? )', {},
		shift(@data), shift(@data),
	);
}
$dbh->commit;

# Issue the group/sum/sort/limit query
my $rv = $dbh->selectall_arrayref(<<'END_SQL');
select mygroup, sum(mynumber) as total
from foo
group by mygroup
order by total
limit 3
END_SQL

is_deeply(
	$rv,
	[
		[ 'a', -1 ],
		[ 'c', 2  ],
		[ 'b', 3  ], 
	],
	'group/sum/sort/limit query ok'
);
