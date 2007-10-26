#!perl -w

use Test::More tests => 8;

$|=1;
$^W=1;

BEGIN { use_ok( 'DBI', ':sql_types' ) }
BEGIN { use_ok( 'DBI::DBD::Metadata' ) } # just to check for syntax errors etc

$dbh = DBI->connect("dbi:ExampleP:.","","", { FetchHashKeyName => 'NAME_lc' })
	or die "Unable to connect to ExampleP driver: $DBI::errstr";

isa_ok($dbh, 'DBI::db');
#$dbh->trace(3);

#use Data::Dumper;
#print Dumper($dbh->type_info_all);
#print Dumper($dbh->type_info);
#print Dumper($dbh->type_info(DBI::SQL_INTEGER));

my @ti = $dbh->type_info;
ok(@ti>0);

is($dbh->type_info(SQL_INTEGER)->{DATA_TYPE}, SQL_INTEGER);
is($dbh->type_info(SQL_INTEGER)->{TYPE_NAME}, 'INTEGER');

is($dbh->type_info(SQL_VARCHAR)->{DATA_TYPE}, SQL_VARCHAR);
is($dbh->type_info(SQL_VARCHAR)->{TYPE_NAME}, 'VARCHAR');

1;
