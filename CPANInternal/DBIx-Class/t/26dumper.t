use strict;
use Test::More;
use IO::File;

use Data::Dumper;
$Data::Dumper::Sortkeys = 1;

use lib qw(t/lib);

BEGIN {
    eval "use DBD::SQLite";
    plan $ENV{DATA_DUMPER_TEST}
        ? ( tests => 2 )
        : ( skip_all => 'Set $ENV{DATA_DUMPER_TEST} to run this test' );
}


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

1;
