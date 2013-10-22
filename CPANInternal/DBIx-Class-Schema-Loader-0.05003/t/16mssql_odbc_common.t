use strict;
use lib qw(t/lib);
use dbixcsl_common_tests;
use dbixcsl_mssql_extra_tests;

my $dsn      = $ENV{DBICTEST_MSSQL_ODBC_DSN} || '';
my $user     = $ENV{DBICTEST_MSSQL_ODBC_USER} || '';
my $password = $ENV{DBICTEST_MSSQL_ODBC_PASS} || '';

my $tester = dbixcsl_common_tests->new(
    vendor      => 'mssql',
    auto_inc_pk => 'INTEGER IDENTITY NOT NULL PRIMARY KEY',
    default_function     => 'getdate()',
    default_function_def => 'DATETIME DEFAULT getdate()',
    dsn         => $dsn,
    user        => $user,
    password    => $password,
    extra       => dbixcsl_mssql_extra_tests->extra,
);

if( !$dsn || !$user ) {
    $tester->skip_tests('You need to set the DBICTEST_MSSQL_ODBC_DSN, _USER, and _PASS environment variables');
}
else {
    $tester->run_tests();
}
