use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 10;

my $old_artistid = 1;
my $new_artistid = $schema->resultset("Artist")->get_column('artistid')->max + 1;

# Update the PK
{
  my $artist = $schema->resultset("Artist")->find($old_artistid);
  ok(defined $artist, 'found an artist with the new PK');

  $artist->update({ artistid => $new_artistid });
  is($artist->artistid, $new_artistid, 'artist ID matches');
}

# Look for the old PK
{
  my $artist = $schema->resultset("Artist")->find($old_artistid);
  ok(!defined $artist, 'no artist found with the old PK');
}

# Look for the new PK
{
  my $artist = $schema->resultset("Artist")->find($new_artistid);
  ok(defined $artist, 'found an artist with the new PK');
  is($artist->artistid, $new_artistid, 'artist ID matches');
}

# Do it all over again, using a different methodology:
$old_artistid = $new_artistid;
$new_artistid++;

# Update the PK
{
  my $artist = $schema->resultset("Artist")->find($old_artistid);
  ok(defined $artist, 'found an artist with the new PK');

  $artist->artistid($new_artistid);
  $artist->update;
  is($artist->artistid, $new_artistid, 'artist ID matches');
}

# Look for the old PK
{
  my $artist = $schema->resultset("Artist")->find($old_artistid);
  ok(!defined $artist, 'no artist found with the old PK');
}

# Look for the new PK
{
  my $artist = $schema->resultset("Artist")->find($new_artistid);
  ok(defined $artist, 'found an artist with the new PK');
  is($artist->artistid, $new_artistid, 'artist ID matches');
}
