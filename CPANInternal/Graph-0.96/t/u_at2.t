use Test::More tests => 4;

use strict;
use Graph;

my $g = Graph::Undirected->new;

while (<DATA>) {
    if (/(\S+)\s+(\S+)/) {
	$g->add_edge($1, $2);
    }
}

my $src = "NRTPZ5WOkg";
my $dst = "ObpULOKHH0";

my @u = qw(NRTPZ5WOkg
	   vJqD6skXdS
	   TNgfs0KcUd
	   qI7Po3TrBA
	   ZiPHVw509v
	   bnDd3VuBpJ
	   ObpULOKHH0);

for (1, 2) {
    print "# finding SP_Dijkstra path between $src and $dst\n";
    my @v = $g->SP_Dijkstra($src, $dst);
    is_deeply(\@v, \@u);
    foreach (@v) {
	print "# $_\n";
    }
    {
	print "# finding APSP_Floyd_Warshall path between $src and $dst\n";
	my $apsp = $g->APSP_Floyd_Warshall();
	my @v = $apsp->path_vertices($src, $dst);
	is_deeply(\@v, \@u);
	foreach (@v) {
	    print "# $_\n";
	}
    }
}

__END__
Cwx0nn09zg pDRu7q707v
ENQH4XaK3o bnuPl9BV2A
J6UG5junOo UNQcGQ7Yxs
J6UG5junOo vZJeF6iWP5
JU5fopQvgK Cqw1sHOUJ1
JU5fopQvgK Cwx0nn09zg
NRTPZ5WOkg Cqw1sHOUJ1
NRTPZ5WOkg vJqD6skXdS
ObpULOKHH0 bnDd3VuBpJ
Ody8vNNKOn bnDd3VuBpJ
Ody8vNNKOn nONYKw3o4X
RlBKE0bWDY p5gUeVx6pZ
UNQcGQ7Yxs els2v8URGW
ZiPHVw509v qI7Po3TrBA
bnDd3VuBpJ ZiPHVw509v
bnuPl9BV2A eiTqtOz3aL
eiTqtOz3aL pDRu7q707v
els2v8URGW IDU5MGPovY
p5gUeVx6pZ IDU5MGPovY
pWZsc88Hfm RlBKE0bWDY
pWZsc88Hfm nONYKw3o4X
qI7Po3TrBA TNgfs0KcUd
vJqD6skXdS TNgfs0KcUd
vZJeF6iWP5 ENQH4XaK3o
