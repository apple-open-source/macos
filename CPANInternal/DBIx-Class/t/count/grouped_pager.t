use strict;
use warnings;

use Test::More;

use lib qw(t/lib);

use DBICTest;

plan tests => 7;

my $schema = DBICTest->init_schema();

# add 2 extra artists
$schema->populate ('Artist', [
    [qw/name/],
    [qw/ar_1/],
    [qw/ar_2/],
]);

# add 3 extra cds to every artist
for my $ar ($schema->resultset ('Artist')->all) {
  for my $cdnum (1 .. 3) {
    $ar->create_related ('cds', {
      title => "bogon $cdnum",
      year => 2000 + $cdnum,
    });
  }
}

my $cds = $schema->resultset ('CD')->search ({}, { group_by => 'artist' } );
is ($cds->count, 5, 'Resultset collapses to 5 groups');

my ($pg1, $pg2, $pg3) = map { $cds->search_rs ({}, {rows => 2, page => $_}) } (1..3);

for ($pg1, $pg2, $pg3) {
  is ($_->pager->total_entries, 5, 'Total count via pager correct');
}

is ($pg1->count, 2, 'First page has 2 groups');
is ($pg2->count, 2, 'Second page has 2 groups');
is ($pg3->count, 1, 'Third page has one group remaining');
