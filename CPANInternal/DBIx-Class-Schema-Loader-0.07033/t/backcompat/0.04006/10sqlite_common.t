use strict;
use lib qw(t/backcompat/0.04006/lib);
use dbixcsl_common_tests;
use dbixcsl_test_dir qw/$tdir/;

use Test::More;
plan skip_all => 'set SCHEMA_LOADER_TESTS_BACKCOMPAT to enable these tests'
    unless $ENV{SCHEMA_LOADER_TESTS_BACKCOMPAT};

eval { require DBD::SQLite };
my $class = $@ ? 'SQLite2' : 'SQLite';

{
    my $tester = dbixcsl_common_tests->new(
        vendor          => 'SQLite',
        auto_inc_pk     => 'INTEGER NOT NULL PRIMARY KEY',
        dsn             => "dbi:$class:dbname=$tdir/sqlite_test",
        user            => '',
        password        => '',
    );

    $tester->run_tests();
}

END {
    unlink "$tdir/sqlite_test" if $ENV{SCHEMA_LOADER_TESTS_BACKCOMPAT};
}
