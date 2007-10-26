use Test::More tests => 16;

use Graph::UnionFind;

my $uf = Graph::UnionFind->new;

is($uf->find('a'), undef);
$uf->add('a');
is($uf->find('a'), 'a');
$uf->add('b');
is($uf->find('a'), 'a');
is($uf->find('b'), 'b');

ok( $uf->union('a', 'b')); # http://rt.cpan.org/NoAuth/Bug.html?id=2627

is($uf->find('a'), 'b');
is($uf->find('b'), 'b');

$uf->union('c', 'd');

is($uf->find('c'), 'd');
is($uf->find('d'), 'd');

is($uf->find('e'), undef);

ok( $uf->same('a', 'b'));
ok( $uf->same('b', 'a'));
ok( $uf->same('c', 'd'));
ok(!$uf->same('a', 'c'));

$uf->union('a', 'd');
ok( $uf->same('a', 'c'));

ok(!$uf->same('c', 'e'));

