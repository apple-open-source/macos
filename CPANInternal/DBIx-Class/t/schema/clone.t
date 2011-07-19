use strict;
use warnings;
use Test::More;

use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

my $clone = $schema->clone;
cmp_ok ($clone->storage, 'eq', $schema->storage, 'Storage copied into new schema (not a new instance)');

done_testing;
