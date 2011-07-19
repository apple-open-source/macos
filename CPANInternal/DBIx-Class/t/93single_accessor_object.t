use strict;
use warnings;  

use Test::More;
use Test::Exception;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 10;

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

$schema = DBICTest->init_schema();

{
	my $artist = $schema->resultset('Artist')->create({ artistid => 666, name => 'bad religion' });
	my $cd = $schema->resultset('CD')->create({ cdid => 187, artist => 1, title => 'how could hell be any worse?', year => 1982, genreid => undef });

	ok(!defined($cd->get_column('genreid')), 'genreid is NULL');  #no accessor was defined for this column
	ok(!defined($cd->genre), 'genre accessor returns undef');
}

$schema = DBICTest->init_schema();

{
	my $artist = $schema->resultset('Artist')->create({ artistid => 666, name => 'bad religion' });
	my $genre = $schema->resultset('Genre')->create({ genreid => 88, name => 'disco' });
	my $cd = $schema->resultset('CD')->create({ cdid => 187, artist => 1, title => 'how could hell be any worse?', year => 1982 });

	dies_ok { $cd->genre } 'genre accessor throws without column';
}

