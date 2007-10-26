# vim: filetype=perl
use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

eval 'require JSON';
plan skip_all => 'Install JSON to run this test' if ($@);

eval 'require Text::CSV_XS';
if ($@) {
    eval 'require Text::CSV_PP';
    plan skip_all => 'Install Text::CSV_XS or Text::CSV_PP to run this test' if ($@);
}

plan tests => 5;

# double quotes round the arguments and single-quote within to make sure the
# tests run on windows as well

my $employees = $schema->resultset('Employee');
my $cmd = qq|$^X script/dbicadmin --schema=DBICTest::Schema --class=Employee --tlibs --connect="['dbi:SQLite:dbname=t/var/DBIxClass.db','','']" --force --tlibs|;

`$cmd --op=insert --set="{name:'Matt'}"`;
ok( ($employees->count()==1), 'insert count' );

my $employee = $employees->find(1);
ok( ($employee->name() eq 'Matt'), 'insert valid' );

`$cmd --op=update --set="{name:'Trout'}"`;
$employee = $employees->find(1);
ok( ($employee->name() eq 'Trout'), 'update' );

`$cmd --op=insert --set="{name:'Aran'}"`;
my $data = `$cmd --op=select --attrs="{order_by:'name'}"`;
ok( ($data=~/Aran.*Trout/s), 'select with attrs' );

`$cmd --op=delete --where="{name:'Trout'}"`;
ok( ($employees->count()==1), 'delete' );

