use strict;
use Test::More;

#----------------------------------------------------------------------
# Test database failures
#----------------------------------------------------------------------

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 2);
}

use lib 't/cdbi/testlib';
use Film;

Film->create({
    title => "Bad Taste",
    numexplodingsheep => 10,
});

Film->create({
    title => "Evil Alien Conquerers",
    numexplodingsheep => 2,
});

is( Film->maximum_value_of("numexplodingsheep"), 10 );
is( Film->minimum_value_of("numexplodingsheep"), 2  );
