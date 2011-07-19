use strict;
use Test::More;

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

use lib qw(t/lib);
use_ok('DBICTest');

my $schema = DBICTest->init_schema();
my $rs = $schema->resultset('CD')->search({
  'artist.name' => 'We Are Goth',
  'liner_notes.notes' => 'Kill Yourself!',
}, {
  join => [ qw/artist liner_notes/ ],
});

Dumper($rs);

$rs = $schema->resultset('CD')->search({
  'artist.name' => 'We Are Goth',
  'liner_notes.notes' => 'Kill Yourself!',
}, {
  join => [ qw/artist liner_notes/ ],
});

cmp_ok( $rs->count(), '==', 1, "Single record in after death with dumper");

done_testing;
