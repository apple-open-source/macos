use strict;
use lib qw(t/backcompat/0.04006/lib);
use dbixcsl_common_tests;
use Test::More;
plan skip_all => 'set SCHEMA_LOADER_TESTS_BACKCOMPAT to enable these tests'
    unless $ENV{SCHEMA_LOADER_TESTS_BACKCOMPAT};


my $dsn         = $ENV{DBICTEST_MYSQL_DSN} || '';
my $user        = $ENV{DBICTEST_MYSQL_USER} || '';
my $password    = $ENV{DBICTEST_MYSQL_PASS} || '';
my $test_innodb = $ENV{DBICTEST_MYSQL_INNODB} || 0;

my $skip_rels_msg = 'You need to set the DBICTEST_MYSQL_INNODB environment variable to test relationships';

my $tester = dbixcsl_common_tests->new(
    vendor           => 'Mysql',
    auto_inc_pk      => 'INTEGER NOT NULL PRIMARY KEY AUTO_INCREMENT',
    innodb           => $test_innodb ? q{Engine=InnoDB} : 0,
    dsn              => $dsn,
    user             => $user,
    password         => $password,
    skip_rels        => $test_innodb ? 0 : $skip_rels_msg,
    no_inline_rels   => 1,
    no_implicit_rels => 1,
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_MYSQL_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
