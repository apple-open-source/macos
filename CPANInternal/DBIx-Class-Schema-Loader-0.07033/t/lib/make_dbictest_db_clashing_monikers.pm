package make_dbictest_db_clashing_monikers;

use strict;
use warnings;
use DBI;
use dbixcsl_test_dir qw/$tdir/;

eval { require DBD::SQLite };
my $class = $@ ? 'SQLite2' : 'SQLite';

my $fn = "$tdir/dbictest_clashing_tables.db";

unlink($fn);
our $dsn = "dbi:$class:dbname=$fn";
my $dbh = DBI->connect($dsn);
$dbh->do('PRAGMA SYNCHRONOUS = OFF');

$dbh->do($_) for (
    q|CREATE TABLE foo (
        fooid INTEGER PRIMARY KEY,
        footext TEXT DEFAULT 'footext',
        foodt TIMESTAMP DEFAULT CURRENT_TIMESTAMP
      )|,
    q|CREATE TABLE bar (
        barid INTEGER PRIMARY KEY,
        fooref INTEGER REFERENCES foo(fooid)
      )|,
# this will cause a singularized moniker clash
    q|CREATE TABLE bars (
        barid INTEGER PRIMARY KEY,
        fooref INTEGER REFERENCES foo(fooid)
      )|,
    q|INSERT INTO foo (fooid, footext) VALUES (1,'Foo text for number 1')|,
    q|INSERT INTO foo (fooid, footext) VALUES (2,'Foo record associated with the Bar with barid 3')|,
    q|INSERT INTO foo (fooid, footext) VALUES (3,'Foo text for number 3')|,
    q|INSERT INTO foo (fooid, footext) VALUES (4,'Foo text for number 4')|,
    q|INSERT INTO bar VALUES (1,4)|,
    q|INSERT INTO bar VALUES (2,3)|,
    q|INSERT INTO bar VALUES (3,2)|,
    q|INSERT INTO bar VALUES (4,1)|,
);

END { unlink($fn) unless $ENV{SCHEMA_LOADER_TESTS_NOCLEANUP}; }

1;
