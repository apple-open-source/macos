use Test::More tests => 1;

use Graph;

my $dump;

my $g0 = Graph->new();

$g0->add_edge('a', 'b');

$dump = $g0->_dump;

$dump =~ s/\s+//sg;
$dump =~ s/'(\d+)'/$1/g;
$dump =~ s/1=>'b',0=>'a'/0=>'a',1=>'b'/g;
$dump =~ s/'b'=>1,'a'=>0/'a'=>0,'b'=>1/g;

is($dump, q[$Graph=bless([0,3,bless([2,152,1,{0=>'a',1=>'b'},{'a'=>0,'b'=>1},{},$Graph],'Graph::AdjacencyMap::Light'),bless([1,128,2,{0=>[0,1]},{0=>{1=>0}},{1=>{0=>0}},$Graph],'Graph::AdjacencyMap::Light')],'Graph');]);

