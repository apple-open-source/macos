use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

plan tests => 9;

my $schema = DBICTest->init_schema();

lives_ok(sub {
  # while cds.* will be selected anyway (prefetch currently forces the result of _resolve_prefetch)
  # only the requested me.name column will be fetched.

  # reference sql with select => [...]
  #   SELECT me.name, cds.title, cds.cdid, cds.artist, cds.title, cds.year, cds.genreid, cds.single_track FROM ...

  my $rs = $schema->resultset('Artist')->search(
    { 'cds.title' => { '!=', 'Generic Manufactured Singles' } },
    {
      prefetch => [ qw/ cds / ],
      order_by => [ { -desc => 'me.name' }, 'cds.title' ],
      select => [qw/ me.name  cds.title / ],
    }
  );

  is ($rs->count, 2, 'Correct number of collapsed artists');
  my $we_are_goth = $rs->first;
  is ($we_are_goth->name, 'We Are Goth', 'Correct first artist');
  is ($we_are_goth->cds->count, 1, 'Correct number of CDs for first artist');
  is ($we_are_goth->cds->first->title, 'Come Be Depressed With Us', 'Correct cd for artist');
}, 'explicit prefetch on a keyless object works');


lives_ok(sub {
  # test implicit prefetch as well

  my $rs = $schema->resultset('CD')->search(
    { title => 'Generic Manufactured Singles' },
    {
      join=> 'artist',
      select => [qw/ me.title artist.name / ],
    }
  );

  my $cd = $rs->next;
  is ($cd->title, 'Generic Manufactured Singles', 'CD title prefetched correctly');
  isa_ok ($cd->artist, 'DBICTest::Artist');
  is ($cd->artist->name, 'Random Boy Band', 'Artist object has correct name');

}, 'implicit keyless prefetch works');
