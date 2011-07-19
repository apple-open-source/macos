use strict;

use Test::More;
use lib 't/cdbi/testlib';

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  plan $@ ? (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@") : (tests=> 24);
}


#-----------------------------------------------------------------------
# Make sure that we can set up columns properly
#-----------------------------------------------------------------------
package State;

use base 'DBIC::Test::SQLite';

State->table('State');
State->columns(Essential => qw/Abbreviation Name/);
State->columns(Primary =>   'Name');
State->columns(Weather =>   qw/Rain Snowfall/);
State->columns(Other =>     qw/Capital Population/);
#State->has_many(cities => "City");

sub accessor_name_for {
  my ($class, $column) = @_;
  my $return = $column eq "Rain" ? "Rainfall" : $column;
  return $return;
}

sub mutator_name_for {
  my ($class, $column) = @_;
  my $return = $column eq "Rain" ? "set_Rainfall" : "set_$column";
  return $return;
}

sub Snowfall { 1 }


package City;

use base 'DBIC::Test::SQLite';

City->table('City');
City->columns(All => qw/Name State Population/);

{
  # Disable the `no such table' warning
  local $SIG{__WARN__} = sub {
    my $warning = shift;
    warn $warning unless ($warning =~ /\Qno such table: City(1)\E/);
  };

  City->has_a(State => 'State');
}

#-------------------------------------------------------------------------
package CD;
use base 'DBIC::Test::SQLite';

CD->table('CD');
CD->columns('All' => qw/artist title length/);

#-------------------------------------------------------------------------

package main;

is(State->table,          'State', 'State table()');
is(State->primary_column, 'name',  'State primary()');
is_deeply [ State->columns('Primary') ] => [qw/name/],
  'State Primary:' . join ", ", State->columns('Primary');
is_deeply [ sort State->columns('Essential') ] => [qw/abbreviation name/],
  'State Essential:' . join ", ", State->columns('Essential');
is_deeply [ sort State->columns('All') ] =>
  [ sort qw/name abbreviation rain snowfall capital population/ ],
  'State All:' . join ", ", State->columns('All');

is(CD->primary_column, 'artist', 'CD primary()');
is_deeply [ CD->columns('Primary') ] => [qw/artist/],
  'CD primary:' . join ", ", CD->columns('Primary');
is_deeply [ sort CD->columns('All') ] => [qw/artist length title/],
  'CD all:' . join ", ", CD->columns('All');
is_deeply [ sort CD->columns('Essential') ] => [qw/artist/],
  'CD essential:' . join ", ", CD->columns('Essential');

ok(State->find_column('Rain'), 'find_column Rain');
ok(State->find_column('rain'), 'find_column rain');
ok(!State->find_column('HGLAGAGlAG'), '!find_column HGLAGAGlAG');

{

    can_ok +State => qw/Rainfall _Rainfall_accessor set_Rainfall
      _set_Rainfall_accessor Snowfall _Snowfall_accessor set_Snowfall
      _set_Snowfall_accessor/;

    foreach my $method (qw/Rain _Rain_accessor rain snowfall/) {
      ok !State->can($method), "State can't $method";
    }

}

{
  SKIP: {
    skip "No column objects", 1;

    eval { my @grps = State->__grouper->groups_for("Huh"); };
    ok $@, "Huh not in groups";
  }

  my @grps = sort State->__grouper->groups_for(State->_find_columns(qw/rain capital/));
  is @grps, 2, "Rain and Capital = 2 groups";
        @grps = sort @grps; # Because the underlying API is hash-based
  is $grps[0], 'Other',   " - Other";
  is $grps[1], 'Weather', " - Weather";
}

#{
#
#        package DieTest;
#        @DieTest::ISA = qw(DBIx::Class);
#        DieTest->load_components(qw/CDBICompat::Retrieve Core/);
#        package main;
#  local $SIG{__WARN__} = sub { };
#  eval { DieTest->retrieve(1) };
#  like $@, qr/unless primary columns are defined/, "Need primary key for retrieve";
#}

#-----------------------------------------------------------------------
# Make sure that columns inherit properly
#-----------------------------------------------------------------------
package State;

package A;
@A::ISA = qw(DBIx::Class);
__PACKAGE__->load_components(qw/CDBICompat Core/);
__PACKAGE__->table('dummy');
__PACKAGE__->columns(Primary => 'id');

package A::B;
@A::B::ISA = 'A';
__PACKAGE__->table('dummy2');
__PACKAGE__->columns(All => qw(id b1));

package A::C;
@A::C::ISA = 'A';
__PACKAGE__->table('dummy3');
__PACKAGE__->columns(All => qw(id c1 c2 c3));

package main;
is join (' ', sort A->columns),    'id',          "A columns";
is join (' ', sort A::B->columns), 'b1 id',       "A::B columns";
is join (' ', sort A::C->columns), 'c1 c2 c3 id', "A::C columns";

