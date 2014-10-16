use Graph;
use strict;

use Test::More tests => 18;

my $g0 = Graph->new (multiedged => 1);

for my $i (0..2) {
  print "# Adding 'A' - 'B'\n";
  my $id = $g0->add_edge_get_id('A', 'B');
  is($id, $i, "id is $i");

  my @ids = sort { $a <=> $b } $g0->get_multiedge_ids('A', 'B');
  print "# ids = @ids\n";
  for my $j (0..$i) {
      is($ids[$j], $j, "id[$j] is $j");
  }
}

my $g1 = Graph->new (multivertexed => 1);

for my $i (0..2) {
  print "# Adding 'C'\n";
  my $id = $g1->add_vertex_get_id('C');
  is($id, $i, "id is $i");

  my @ids = sort { $a <=> $b } $g1->get_multivertex_ids('C');
  print "# ids = @ids\n";
  for my $j (0..$i) {
      is($ids[$j], $j, "id[$j] is $j");
  }
}
