use strict;
use Test::More;

BEGIN {
  eval "use DBIx::Class::CDBICompat;";
  if ($@) {
    plan (skip_all => 'Class::Trigger and DBIx::ContextualFetch required');
    next;
  }
  eval "use DBD::SQLite";
  plan $@ ? (skip_all => 'needs DBD::SQLite for testing') : (tests => 22);
}

use lib 't/cdbi/testlib';
use Film;
use Blurb;

is(Blurb->primary_column, "title", "Primary key of Blurb = title");
is_deeply [ Blurb->columns('Essential') ], [ Blurb->primary_column ], "Essential = Primary";

eval { Blurb->retrieve(10) };
is $@, "", "No problem retrieving non-existent Blurb";

Film->might_have(info => Blurb => qw/blurb/);

Film->create_test_film;

{
  ok my $bt = Film->retrieve('Bad Taste'), "Get Film";
  isa_ok $bt, "Film";
  is $bt->info, undef, "No blurb yet";
  # bug where we couldn't write a class with a might_have that didn't_have
  $bt->rating(16);
  eval { $bt->update };
  is $@, '', "No problems updating when don't have";
  is $bt->rating, 16, "Updated OK";

  is $bt->blurb, undef, "Bad taste has no blurb";
  $bt->blurb("Wibble bar");
  $bt->update;
  is $bt->blurb, "Wibble bar", "And we can write the info";
}

{
  my $bt   = Film->retrieve('Bad Taste');
  my $info = $bt->info;
  isa_ok $info, 'Blurb';

  is $bt->blurb, $info->blurb, "Blurb is the same as fetching the long way";
  ok $bt->blurb("New blurb"), "We can set the blurb";
  $bt->update;
  is $bt->blurb, $info->blurb, "Blurb has been set";

  $bt->rating(18);
  eval { $bt->update };
  is $@, '', "No problems updating when do have";
  is $bt->rating, 18, "Updated OK";

  # cascade delete?
  {
    my $blurb = Blurb->retrieve('Bad Taste');
    isa_ok $blurb => "Blurb";
    $bt->delete;
    $blurb = Blurb->retrieve('Bad Taste');
    is $blurb, undef, "Blurb has gone";
  }
    
}

{
    my $host = Film->create({ title => "Gwoemul" });
    $host->blurb("Monsters are real.");
    my $info = $host->info;
    is $info->blurb, "Monsters are real.";

    $host->discard_changes;
    is $host->info->id, $info->id,
        'relationships still valid after discard_changes';

    ok $host->info->delete;
    $host->discard_changes;
    ok !$host->info, 'relationships rechecked after discard_changes';
}