# vim: filetype=perl
use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval 'require JSON::Any';
plan skip_all => 'Install JSON::Any to run this test' if ($@);

eval 'require Text::CSV_XS';
if ($@) {
    eval 'require Text::CSV_PP';
    plan skip_all => 'Install Text::CSV_XS or Text::CSV_PP to run this test' if ($@);
}

plan tests => 5;

# the script supports double quotes round the arguments and single-quote within
# to make sure it runs on windows as well, but only if JSON::Any picks the right module



my $employees = $schema->resultset('Employee');
my @cmd = ($^X, qw|script/dbicadmin --quiet --schema=DBICTest::Schema --class=Employee --tlibs|, q|--connect=["dbi:SQLite:dbname=t/var/DBIxClass.db","","",{"AutoCommit":1}]|, qw|--force --tlibs|);

system(@cmd, qw|--op=insert --set={"name":"Matt"}|);
ok( ($employees->count()==1), 'insert count' );

my $employee = $employees->find(1);
ok( ($employee->name() eq 'Matt'), 'insert valid' );

system(@cmd, qw|--op=update --set={"name":"Trout"}|);
$employee = $employees->find(1);
ok( ($employee->name() eq 'Trout'), 'update' );

system(@cmd, qw|--op=insert --set={"name":"Aran"}|);

open(my $fh, "-|", @cmd, qw|--op=select --attrs={"order_by":"name"}|) or die $!;
my $data = do { local $/; <$fh> };
close($fh);
ok( ($data=~/Aran.*Trout/s), 'select with attrs' );

system(@cmd, qw|--op=delete --where={"name":"Trout"}|);
ok( ($employees->count()==1), 'delete' );

