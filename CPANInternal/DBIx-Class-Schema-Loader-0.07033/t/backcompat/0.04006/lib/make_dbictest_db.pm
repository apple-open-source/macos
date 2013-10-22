package make_dbictest_db;

use strict;
use warnings;
use DBI;
use dbixcsl_test_dir qw/$tdir/;

eval { require DBD::SQLite };
my $class = $@ ? 'SQLite2' : 'SQLite';

my $fn = "$tdir/dbictest.db";

unlink($fn);
our $dsn = "dbi:$class:dbname=$fn";
my $dbh = DBI->connect($dsn);
$dbh->do ('PRAGMA SYNCHRONOUS = OFF');

$dbh->do($_) for (
    q|CREATE TABLE foo (
        fooid INTEGER PRIMARY KEY,
        footext TEXT
      )|,
    q|CREATE TABLE bar (
        barid INTEGER PRIMARY KEY,
        fooref INTEGER REFERENCES foo(fooid)
      )|,
    q|INSERT INTO foo VALUES (1,'Foo text for number 1')|,
    q|INSERT INTO foo VALUES (2,'Foo record associated with the Bar with barid 3')|,
    q|INSERT INTO foo VALUES (3,'Foo text for number 3')|,
    q|INSERT INTO foo VALUES (4,'Foo text for number 4')|,
    q|INSERT INTO bar VALUES (1,4)|,
    q|INSERT INTO bar VALUES (2,3)|,
    q|INSERT INTO bar VALUES (3,2)|,
    q|INSERT INTO bar VALUES (4,1)|,
);

END { unlink($fn); }

1;
