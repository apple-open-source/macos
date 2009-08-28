use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 7;

# Test various uses of passing an object to find, create, and update on a single
# rel accessor
{
  my $artist = $schema->resultset("Artist")->find(1);

  my $cd = $schema->resultset("CD")->find_or_create({
    artist => $artist,
    title  => "Object on a might_have",
    year   => 2006,
  });
  ok(defined $cd, 'created a CD');
  is($cd->get_column('artist'), $artist->id, 'artist matches CD');

  my $liner_notes = $schema->resultset("LinerNotes")->find_or_create({
    cd     => $cd,
    notes  => "Creating using an object on a might_have is helpful.",
  });
  ok(defined $liner_notes, 'created liner notes');
  is($liner_notes->liner_id, $cd->cdid, 'liner notes matches CD');
  is($liner_notes->notes, "Creating using an object on a might_have is helpful.", 'liner notes are correct');

  my $track = $cd->tracks->find_or_create({
    position => 127,
    title    => 'Single Accessor'
  });
  is($track->get_column('cd'), $cd->cdid, 'track matches CD before update');

  my $another_cd = $schema->resultset("CD")->find(5);
  $track->update({ disc => $another_cd });
  is($track->get_column('cd'), $another_cd->cdid, 'track matches another CD after update');
}
