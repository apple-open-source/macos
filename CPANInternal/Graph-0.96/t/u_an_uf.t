# rt.cpan.org #39805: UnionFind: Repeated adds clobbers graph component information

use strict;

use Test::More tests => 4;

use Graph;

my $graph = Graph::UnionFind->new;
$graph->add('a');
$graph->union('a','b');

ok($graph->same('a', 'b'));
ok($graph->same('b', 'a'));

$graph->add('a');

ok($graph->same('a', 'b'));
ok($graph->same('b', 'a'));

