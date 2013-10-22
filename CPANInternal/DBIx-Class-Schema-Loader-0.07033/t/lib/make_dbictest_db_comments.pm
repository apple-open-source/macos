package make_dbictest_db_comments;

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
$dbh->do('PRAGMA SYNCHRONOUS = OFF');

$dbh->do($_) for (
    q|CREATE TABLE table_comments (
        id INTEGER PRIMARY KEY,
        table_name TEXT,
        comment_text TEXT
    )|,
    q|CREATE TABLE column_comments (
        id INTEGER PRIMARY KEY,
        table_name TEXT,
        column_name TEXT,
        comment_text TEXT
    )|,
    q|CREATE TABLE foo (
        fooid INTEGER PRIMARY KEY,
        footext TEXT DEFAULT 'footext',
        foodt TIMESTAMP DEFAULT CURRENT_TIMESTAMP
      )|,
    q|CREATE TABLE bar (
        barid INTEGER PRIMARY KEY,
        fooref INTEGER REFERENCES foo(fooid)
      )|,
    q|INSERT INTO table_comments (id, table_name, comment_text)
        VALUES (1, 'foo', 'a short comment')
     |,
    q|INSERT INTO table_comments (id, table_name, comment_text)
        VALUES (2, 'bar', 'a | . ('very ' x 80) . q|long comment')
     |,
    q|INSERT INTO column_comments (id, table_name, column_name, comment_text)
        VALUES (1, 'foo', 'fooid', 'a short comment')
     |,
    q|INSERT INTO column_comments (id, table_name, column_name, comment_text)
        VALUES (2, 'foo', 'footext', 'a | . ('very ' x 80) . q|long comment')
     |,
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

1
