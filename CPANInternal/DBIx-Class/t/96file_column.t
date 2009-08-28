use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use IO::File;

my $schema = DBICTest->init_schema();

plan tests => 1;

my $fh = new IO::File('t/96file_column.t','r');
eval { $schema->resultset('FileColumn')->create({file => {handle => $fh, filename =>'96file_column.t'}})};
cmp_ok($@,'eq','','FileColumn checking if file handled properly.');
