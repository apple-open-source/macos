use Test::More tests => 11;

use strict;
use warnings;

use Graph;

my ($u, $v);

my $g1 = Graph->new(refvertexed => 1);
use Math::Complex qw(cplx);
$g1->add_edge($u = cplx(1,2), $v = cplx(3,4));
is($g1, "$u-$v");
$g1->delete_vertex($u);
is($g1, $v);
$g1->delete_vertex($v);
is($g1, "");

my $g2 = Graph->new(refvertexed => 1);
use Math::Complex qw(cplx);
$g2->add_vertex($u = cplx(1,2));
is($g2, $u);
$g2->add_vertex($v = cplx(3,4));
is($g2, "$u,$v");
$g2->delete_vertex($u);
is($g2, $v);
$g2->delete_vertex($v);
is($g2, "");

my $g3 = Graph->new(refvertexed => 1);
use Math::Complex qw(cplx);
$g3->add_edge($u = cplx(1,2), $v = cplx(3,4));
is($g3, "$u-$v");
$g3->delete_edge($u, $v);
is($g3, "$u,$v");
$g3->delete_vertex($u);
is($g3, $v);
$g3->delete_vertex($v);
is($g3, "");





