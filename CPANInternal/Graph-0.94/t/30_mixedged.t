use Test::More tests => 96;
	
use Graph;

for my $m (0, 1) {
    for my $r (0, 1) {
	for my $h (0, 1) {
	    my $g = Graph->new(countedged => $m,
			       hyperedged => $h);
	    print "# m = $m, c = $r, h = $h\n";
	    $g->omnidirected($r);
	    $g->add_edge("a", "b");
	    $g->add_edge("a", "b");
	    if ($g->hyperedged) {
		$g->add_edge("c", "d", "e");
		$g->add_edge("c", "d", "e");
	    }
	    for (1, 2) {
		ok(  $g->has_vertices( ) );
		ok(  $g->has_edge("a", "b") );
		if ($g->hyperedged) {
		    ok(  $g->has_edge("c", "d", "e") );
		}
		ok( !$g->has_edge("e", "f") );
	    }
	    for (1, 2) {
		is( $g->get_edge_count("a", "b"),      $m ? 2 : 1 );
		if ($g->hyperedged) {
		    is( $g->get_edge_count("c", "d", "e"), $m ? 2 : 1 );
		}
		is( $g->get_edge_count("e", "f"),      0 );
	    }
	}
    }
}

