use strict;
use Test::More;
use lib 't/cdbi/testlib';

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@")
          : (tests=> 5);
}

{
    package Thing;

    use base 'DBIC::Test::SQLite';

    Thing->columns(TEMP => qw[foo bar]);
    Thing->columns(All  => qw[thing_id yarrow flower]);
    sub foo { 42 }
    sub yarrow { "hock" }
}

is_deeply( [sort Thing->columns("TEMP")],
           [sort qw(foo bar)],
           "TEMP columns set"
);
my $thing = Thing->construct(
    { thing_id => 23, foo => "this", bar => "that" }
);

is( $thing->id, 23 );
is( $thing->yarrow, "hock", 'custom accessor not overwritten by column' );
is( $thing->foo, 42, 'custom routine not overwritten by temp column' );
is( $thing->bar, "that", 'temp column accessor generated' );
