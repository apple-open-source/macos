#!perl -T

# the above line forces Test::Harness into taint-mode

use strict;
use warnings;

our @plan;

BEGIN {
  eval "require Module::Find;";
  @plan = $@ ? ( skip_all => 'Could not load Module::Find' )
    : ( tests => 2 );
}

package DBICTest::Schema;

# Use the default test class namespace to avoid the need for a
# new test infrastructure. If invalid classes will be introduced to
# 't/lib/DBICTest/Schema/' someday, this has to be reworked.

use lib qw(t/lib);

use Test::More @plan;

use base qw/DBIx::Class::Schema/;

eval{ __PACKAGE__->load_classes() };
cmp_ok( $@, 'eq', '',
        'Loading classes with Module::Find worked in taint mode' );
ok( __PACKAGE__->sources(), 'At least on source has been registered' );

1;
