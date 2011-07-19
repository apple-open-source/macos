use strict;

use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : ('no_plan');
}


{
    package Thing;
    use base qw(DBIx::Class::CDBICompat);
}

{
    package Stuff;
    use base qw(DBIx::Class::CDBICompat);
}

# There was a bug where looking at a column group before any were
# set would cause them to be shared across classes.
is_deeply [Stuff->columns("Essential")], [];
Thing->columns(Essential => qw(foo bar baz));
is_deeply [Stuff->columns("Essential")], [];

1;
