use Test::More tests => 20;

use Graph::Undirected;
use Graph::Directed;

my $g0 = Graph::Undirected->new;
my $g1 = Graph::Directed->new;

$g0->add_edge(qw(a b)); $g1->add_edge(qw(a b));
$g0->add_edge(qw(a c)); $g1->add_edge(qw(a c));
$g0->add_edge(qw(a d)); $g1->add_edge(qw(a d));
$g0->add_edge(qw(a e)); $g1->add_edge(qw(a e));
$g0->add_edge(qw(a f)); $g1->add_edge(qw(a f));

$g0->add_edge(qw(b c)); $g1->add_edge(qw(b c));
$g0->add_edge(qw(b d)); $g1->add_edge(qw(b d));
$g0->add_edge(qw(b e)); $g1->add_edge(qw(b e));
$g0->add_edge(qw(b f)); $g1->add_edge(qw(b f));

$g0->add_edge(qw(c b)); $g1->add_edge(qw(c b));
$g0->add_edge(qw(d b)); $g1->add_edge(qw(d b));
$g0->add_edge(qw(e b)); $g1->add_edge(qw(e b));
$g0->add_edge(qw(f b)); $g1->add_edge(qw(f b));

$g0->add_edge(qw(d c)); $g1->add_edge(qw(d c));
$g0->add_edge(qw(e c)); $g1->add_edge(qw(e c));
$g0->add_edge(qw(f c)); $g1->add_edge(qw(f c));

$g0->add_edge(qw(d e)); $g1->add_edge(qw(d e));
$g0->add_edge(qw(d f)); $g1->add_edge(qw(d f));

$g0->add_edge(qw(e f)); $g1->add_edge(qw(e f));

is($g0, 'a=b,a=c,a=d,a=e,a=f,b=c,b=d,b=e,b=f,c=d,c=e,c=f,d=e,d=f,e=f')
    for 1..10;
is($g1, 'a-b,a-c,a-d,a-e,a-f,b-c,b-d,b-e,b-f,c-b,d-b,d-c,d-e,d-f,e-b,e-c,e-f,f-b,f-c')
    for 1..10;

