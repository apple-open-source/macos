use Test::More;
use strict;
use warnings;
use lib qw(t/lib);
use DBICTest;

plan tests => 7;

my $schema = DBICTest->init_schema();
my $total_cds = $schema->resultset('CD')->count;
cmp_ok($total_cds, '>', 0, 'need cd records');

# test that delete_related w/o conditions deletes all related records only
my $artist = $schema->resultset("Artist")->find(3);
my $artist_cds = $artist->cds->count;
cmp_ok($artist_cds, '<', $total_cds, 'need more cds than just related cds');

ok($artist->delete_related('cds'));
cmp_ok($schema->resultset('CD')->count, '==', ($total_cds - $artist_cds), 'too many cds were deleted');

$total_cds -= $artist_cds;

# test that delete_related w/conditions deletes just the matched related records only
my $artist2 = $schema->resultset("Artist")->find(2);
my $artist2_cds = $artist2->search_related('cds')->count;
cmp_ok($artist2_cds, '<', $total_cds, 'need more cds than related cds');

ok($artist2->delete_related('cds', {title => {like => '%'}}));
cmp_ok($schema->resultset('CD')->count, '==', ($total_cds - $artist2_cds), 'too many cds were deleted');

