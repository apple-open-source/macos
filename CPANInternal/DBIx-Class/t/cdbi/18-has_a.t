use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 41);
}

use lib 't/cdbi/testlib';
use Film;
use Director;
@YA::Film::ISA = 'Film';

Film->create_test_film;

ok my $btaste = Film->retrieve('Bad Taste'), "We have Bad Taste";
ok my $pj = $btaste->Director, "Bad taste has a director";
ok !ref($pj), ' ... which is not an object';

ok(Film->has_a('Director' => 'Director'), "Link Director table");
ok(
  Director->create({
      Name     => 'Peter Jackson',
      Birthday => -300000000,
      IsInsane => 1
    }
  ),
  'create Director'
);

{
  ok $btaste = Film->retrieve('Bad Taste'), "Reretrieve Bad Taste";
  ok $pj = $btaste->Director, "Bad taste now hasa() director";
  isa_ok $pj => 'Director';
  {
    no warnings qw(redefine once);
    local *Ima::DBI::st::execute =
      sub { ::fail("Shouldn't need to query db"); };
    is $pj->id, 'Peter Jackson', 'ID already stored';
  }
  ok $pj->IsInsane, "But we know he's insane";
}

# Oh no!  Its Peter Jacksons even twin, Skippy!  Born one minute after him.
my $sj = Director->create({
    Name     => 'Skippy Jackson',
    Birthday => (-300000000 + 60),
    IsInsane => 1,
  });

{
  eval { $btaste->Director($btaste) };
  like $@, qr/Director/, "Can't set film as director";
  is $btaste->Director->id, $pj->id, "PJ still the director";

  # drop from cache so that next retrieve() is from db
  $btaste->remove_from_object_index;
}

{    # Still inflated after update
  my $btaste = Film->retrieve('Bad Taste');
  isa_ok $btaste->Director, "Director";
  $btaste->numexplodingsheep(17);
  $btaste->update;
  isa_ok $btaste->Director, "Director";

  $btaste->Director('Someone Else');
  $btaste->update;
  isa_ok $btaste->Director, "Director";
  is $btaste->Director->id, "Someone Else", "Can change director";
}

is $sj->id, 'Skippy Jackson', 'Create new director - Skippy';
Film->has_a('CoDirector' => 'Director');
{
  eval { $btaste->CoDirector("Skippy Jackson") };
  is $@, "", "Auto inflates";
  isa_ok $btaste->CoDirector, "Director";
  is $btaste->CoDirector->id, $sj->id, "To skippy";
}

$btaste->CoDirector($sj);
$btaste->update;
is($btaste->CoDirector->Name, 'Skippy Jackson', 'He co-directed');
is(
  $btaste->Director->Name,
  'Peter Jackson',
  "Didnt interfere with each other"
);

{    # Inheriting hasa
  my $btaste = YA::Film->retrieve('Bad Taste');
  is(ref($btaste->Director),    'Director',       'inheriting hasa()');
  is(ref($btaste->CoDirector),  'Director',       'inheriting hasa()');
  is($btaste->CoDirector->Name, 'Skippy Jackson', ' ... correctly');
}

{
  $sj = Director->retrieve('Skippy Jackson');
  $pj = Director->retrieve('Peter Jackson');

  my $fail;
  eval {
    $fail = YA::Film->create({
        Title             => 'Tastes Bad',
        Director          => $sj,
        codirector        => $btaste,
        Rating            => 'R',
        NumExplodingSheep => 23
      });
  };
  ok $@,    "Can't have film as codirector: $@";
  is $fail, undef, "We didn't get anything";

  my $tastes_bad = YA::Film->create({
      Title             => 'Tastes Bad',
      Director          => $sj,
      codirector        => $pj,
      Rating            => 'R',
      NumExplodingSheep => 23
    });
  is($tastes_bad->Director->Name, 'Skippy Jackson', 'Director');
  is(
    $tastes_bad->_director_accessor->Name,
    'Skippy Jackson',
    'director_accessor'
  );
  is($tastes_bad->codirector->Name, 'Peter Jackson', 'codirector');
  is(
    $tastes_bad->_codirector_accessor->Name,
    'Peter Jackson',
    'codirector_accessor'
  );
}

SKIP: {
        skip "Non-standard CDBI relationships not supported by compat", 9;
  {

    YA::Film->add_relationship_type(has_a => "YA::HasA");

    package YA::HasA;
    #use base 'Class::DBI::Relationship::HasA';

    sub _inflator {
      my $self  = shift;
      my $col   = $self->accessor;
      my $super = $self->SUPER::_inflator($col);

      return $super
        unless $col eq $self->class->find_column('Director');

      return sub {
        my $self = shift;
        $self->_attribute_store($col, 'Ghostly Peter')
          if $self->_attribute_exists($col)
          and not defined $self->_attrs($col);
        return &$super($self);
      };
    }
  }
  {

    package Rating;

    sub new {
      my ($class, $mpaa, @details) = @_;
      bless {
        MPAA => $mpaa,
        WHY  => "@details"
      }, $class;
    }
    sub mpaa { shift->{MPAA}; }
    sub why  { shift->{WHY}; }
  }
  local *Director::mapme = sub {
    my ($class, $val) = @_;
    $val =~ s/Skippy/Peter/;
    $val;
  };
  no warnings 'once';
  local *Director::sanity_check = sub { $_[0]->IsInsane ? undef: $_[0] };
  YA::Film->has_a(
    director => 'Director',
    inflate  => 'mapme',
    deflate  => 'sanity_check'
  );
  YA::Film->has_a(
    rating  => 'Rating',
    inflate => sub {
      my ($val, $parent) = @_;
      my $sheep = $parent->find_column('NumexplodingSheep');
      if ($parent->_attrs($sheep) || 0 > 20) {
        return new Rating 'NC17', 'Graphic ovine violence';
      } else {
        return new Rating $val, 'Just because';
      }
    },
    deflate => sub {
      shift->mpaa;
    });

  my $tbad = YA::Film->retrieve('Tastes Bad');

  isa_ok $tbad->Director, 'Director';
  is $tbad->Director->Name, 'Peter Jackson', 'Director shuffle';
  $tbad->Director('Skippy Jackson');
  $tbad->update;
  is $tbad->Director, 'Ghostly Peter', 'Sanity checked';

  isa_ok $tbad->Rating, 'Rating';
  is $tbad->Rating->mpaa, 'NC17', 'Rating bumped';
  $tbad->Rating(new Rating 'NS17', 'Shaken sheep');
  no warnings 'redefine';
  local *Director::mapme = sub {
    my ($class, $obj) = @_;
    $obj->isa('Film') ? $obj->Director : $obj;
  };

  $pj->IsInsane(0);
  $pj->update;    # Hush warnings

  ok $tbad->Director($btaste), 'Cross-class mapping';
  is $tbad->Director, 'Peter Jackson', 'Yields PJ';
  $tbad->update;

  $tbad = Film->retrieve('Tastes Bad');
  ok !ref($tbad->Rating), 'Unmagical rating';
  is $tbad->Rating, 'NS17', 'but prior change stuck';
}

{ # Broken has_a declaration
  eval { Film->has_a(driector => "Director") };
  like $@, qr/driector/, "Sensible error from has_a with incorrect column: $@";
}
