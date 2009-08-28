use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

plan tests => 2;

# Set up the "usual" sqlite for DBICTest
my $schema = DBICTest->init_schema;

my $sth_one = $schema->storage->sth('SELECT 42');
my $sth_two = $schema->storage->sth('SELECT 42');
$schema->storage->disable_sth_caching(1);
my $sth_three = $schema->storage->sth('SELECT 42');

ok($sth_one == $sth_two, "statement caching works");
ok($sth_two != $sth_three, "disabling statement caching works");
