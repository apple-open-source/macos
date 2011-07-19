use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@");
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 37);
}

use lib 't/cdbi/testlib';
use Film;

my $it_class = "DBIx::Class::ResultSet";

my @film  = (
  Film->create({ Title => 'Film 1' }),
  Film->create({ Title => 'Film 2' }),
  Film->create({ Title => 'Film 3' }),
  Film->create({ Title => 'Film 4' }),
  Film->create({ Title => 'Film 5' }),
  Film->create({ Title => 'Film 6' }),
);

{
  my $it1 = Film->retrieve_all;
  isa_ok $it1, $it_class;

  my $it2 = Film->retrieve_all;
  isa_ok $it2, $it_class;

  while (my $from1 = $it1->next) {
    my $from2 = $it2->next;
    is $from1->id, $from2->id, "Both iterators get $from1";
  }
}

{
  my $it = Film->retrieve_all;
  is $it->first->title, "Film 1", "Film 1 first";
  is $it->next->title, "Film 2", "Film 2 next";
  is $it->first->title, "Film 1", "First goes back to 1";
  is $it->next->title, "Film 2", "With 2 still next";
  $it->reset;
  is $it->next->title, "Film 1", "Reset brings us to film 1 again";
  is $it->next->title, "Film 2", "And 2 is still next";
}


{
  my $it = Film->retrieve_all;
  my @slice = $it->slice(2,4);
  is @slice, 3, "correct slice size (array)";
  is $slice[0]->title, "Film 3", "Film 3 first";
  is $slice[2]->title, "Film 5", "Film 5 last";
}

{
  my $it = Film->retrieve_all;
  my $slice = $it->slice(2,4);
  isa_ok $slice, $it_class, "slice as iterator";
  is $slice->count, 3,"correct slice size (array)";
  is $slice->first->title, "Film 3", "Film 3 first";
  is $slice->next->title, "Film 4", "Film 4 next";
  is $slice->first->title, "Film 3", "First goes back to 3";
  is $slice->next->title, "Film 4", "With 4 still next";
  $slice->reset;
  is $slice->next->title, "Film 3", "Reset brings us to film 3 again";
  is $slice->next->title, "Film 4", "And 4 is still next";

  # check if the original iterator still works
  is $it->count, 6, "back to the original iterator, is of right size";
  is $it->first->title, "Film 1", "Film 1 first";
  is $it->next->title, "Film 2", "Film 2 next";
  is $it->first->title, "Film 1", "First goes back to 1";
  is $it->next->title, "Film 2", "With 2 still next";
  is $it->next->title, "Film 3", "Film 3 is still in original Iterator";
  $it->reset;
  is $it->next->title, "Film 1", "Reset brings us to film 1 again";
  is $it->next->title, "Film 2", "And 2 is still next";
}

{
  my $it = Film->retrieve_all;
  is $it, $it->count, "iterator returns count as a scalar";
  ok $it, "iterator returns true when there are results";
}

{
  my $it = Film->search( Title => "something which does not exist" );
  is $it, 0;
  ok !$it, "iterator returns false when no results";
}
