use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

# this test will check to see if you can have 2 columns
# in the same class pointing at the same other class
#
# example:
#
# +---------+       +--------------+
# | SelfRef |       | SelfRefAlias |
# +---------+  1-M  +--------------+
# | id      |-------| self_ref     | --+
# | name    |       | alias        | --+
# +---------+       +--------------+   |
#    /|\                               |
#     |                                |
#     +--------------------------------+
#
# see http://use.perl.org/~LTjake/journal/24876 for the
# issue with CDBI

plan tests => 4;

my $item = $schema->resultset("SelfRef")->find( 1 );
is( $item->name, 'First', 'proper start item' );

my @aliases = $item->aliases;

is( scalar @aliases, 1, 'proper number of aliases' );

my $orig  = $aliases[ 0 ]->self_ref;
my $alias = $aliases[ 0 ]->alias;

is( $orig->name, 'First', 'proper original' );
is( $alias->name, 'Second', 'proper alias' );

