use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 4;
my $artist = $schema->resultset('Artist')->find(1);
my $artist_cds = $artist->search_related('cds');

my $cover_band = $artist->copy;

my $cover_cds = $cover_band->search_related('cds');
cmp_ok($cover_band->id, '!=', $artist->id, 'ok got new column id...');
is($cover_cds->count, $artist_cds->count, 'duplicated rows count ok');

#check multi-keyed
cmp_ok($cover_band->search_related('twokeys')->count, '>', 0, 'duplicated multiPK ok');

#and check copying a few relations away
cmp_ok($cover_cds->search_related('tags')->count, '==',
   $artist_cds->search_related('tags')->count , 'duplicated count ok');

