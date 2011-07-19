use strict;
use lib qw(t/lib);
use dbixcsl_common_tests;

# The default max_cursor_count and max_statement_count settings of 50 are too
# low to run this test.

my $dsn      = $ENV{DBICTEST_SYBASE_ASA_ODBC_DSN} || '';
my $user     = $ENV{DBICTEST_SYBASE_ASA_ODBC_USER} || '';
my $password = $ENV{DBICTEST_SYBASE_ASA_ODBC_PASS} || '';

my $tester = dbixcsl_common_tests->new(
    vendor      => 'SQLAnywhere',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    default_function => 'current timestamp',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
);

if( !$dsn ) {
    $tester->skip_tests('You need to set the DBICTEST_SYBASE_ASA_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
