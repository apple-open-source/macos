#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 12;
use Test::NoWarnings;

my $dbh = DBI->connect('dbi:SQLite:dbname=:memory:',undef,undef,{RaiseError => 1});

# 1. Create a table
ok( $dbh->do(<<'END_SQL'), 'Created test table' );
    CREATE TABLE test (
        id INTEGER PRIMARY KEY NOT NULL,
        name VARCHAR(255)
    );
END_SQL

# 2. Create a temporary table
ok( $dbh->do(<<'END_SQL'), 'Created temp test table' );
    CREATE TEMP TABLE test2 (
        id INTEGER PRIMARY KEY NOT NULL,
        flag INTEGER
    );
END_SQL

# 3. Attach a memory database
ok( $dbh->do('ATTACH DATABASE ":memory:" AS db3'), 'ATTACH DATABASE ":memory:" AS db3' );

# 4. Create a table on the attached database
ok( $dbh->do(<<'END_SQL'), 'CREATE TABLE db3.three' );
    CREATE TABLE db3.three (
        id INTEGER NOT NULL,
        name CHAR (64) NOT NULL
    )
END_SQL

# 5. No errors from column_info()
my $sth = $dbh->column_info(undef, undef, 'test', undef);
is $@, '', 'No error creating the table';

# 6. Get column information
ok $sth, 'We can get column information';

my %expected = (
    TYPE_NAME   => [qw( INTEGER VARCHAR )],
    COLUMN_NAME => [qw( id name )],
);

SKIP: {
    skip( "The table didn't get created correctly or we can't get column information.", 5 ) unless $sth;

    my $info = $sth->fetchall_arrayref({});

    # 7. Found 2 columns
    is( scalar @$info, 2, 'We got information on two columns' );

    foreach my $item (qw( TYPE_NAME COLUMN_NAME )) {
        my @info = map { $_->{$item} } (@$info);
        is_deeply( \@info, $expected{$item}, "We got the right info in $item" );
    }

    $info = $dbh->column_info(undef, undef, 't%', '%a%')->fetchall_arrayref({});

    # 10. Found 3 columns
    is( scalar @$info, 3, 'We matched information from multiple databases' );

    my @fields = qw( TABLE_SCHEM TYPE_NAME COLUMN_NAME COLUMN_SIZE NULLABLE );
    my @info = map [ @$_{@fields} ], @$info;
    my $expected = [
        [ 'db3', 'CHAR', 'name', 64, 0 ],
        [ 'main', 'VARCHAR', 'name', 255, 1 ],
        [ 'temp', 'INTEGER', 'flag', undef, 1 ] # TODO: column_info should always return a valid COLUMN_SIZE
    ];

    # 11. Correct info retrieved
    is_deeply( \@info, $expected, 'We got the right info from multiple databases' );
}
