use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => 'Class::Trigger and DBIx::ContextualFetch required')
          : (tests=> 2);
}

package Foo;

use base qw(DBIx::Class::CDBICompat);

eval {
    Foo->table("foo");
    Foo->columns(Essential => qw(foo bar));
    #Foo->has_a( bar => "This::Does::Not::Exist::Yet" );
};
#::is $@, '';
::is(Foo->table, "foo");
::is_deeply [sort map lc, Foo->columns], [sort map lc, qw(foo bar)];
