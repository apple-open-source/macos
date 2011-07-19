use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => 'Class::Trigger and DBIx::ContextualFetch required') : (tests=> 2);
}

use strict;

use lib 't/cdbi/testlib';
use Actor;
use ActorAlias;
Actor->has_many( aliases => [ 'ActorAlias' => 'alias' ] );

my $first  = Actor->create( { Name => 'First' } );
my $second = Actor->create( { Name => 'Second' } );

ActorAlias->create( { actor => $first, alias => $second } );

my @aliases = $first->aliases;

is( scalar @aliases, 1, 'proper number of aliases' );
is( $aliases[ 0 ]->name, 'Second', 'proper alias' );


