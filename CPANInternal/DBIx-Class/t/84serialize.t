use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;
use Storable;

my $schema = DBICTest->init_schema();

plan tests => 6;

my $artist = $schema->resultset('Artist')->find(1);

{
  my $copy = $schema->dclone($artist);
  is_deeply($copy, $artist, "dclone row object works");
  eval { $copy->discard_changes };
  ok( !$@, "discard_changes okay" );
  is($copy->id, $artist->id, "IDs still match ");
}

{
  my $ice = $schema->freeze($artist);
  my $copy = $schema->thaw($ice);
  is_deeply($copy, $artist, 'dclone row object works');

  eval { $copy->discard_changes };
  ok( !$@, "discard_changes okay" );
  is($copy->id, $artist->id, "IDs still okay");
}

