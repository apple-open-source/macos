use Test::More tests => 128;
	
use Graph;

for my $m (0, 1) {
    for my $r (0, 1) {
	for my $h (0, 1) {
	    my $g = Graph->new(countvertexed => $m,
			       refvertexed   => $r,
			       hypervertexed => $h);
	    print "# m = $m, c = $r, h = $h\n";
	    $g->add_vertex("a");
	    $g->add_vertex("a");
	    $g->add_vertex(my $b = []);
	    $g->add_vertex($b);
	    if ($g->hypervertexed) {
		$g->add_vertex("c", "d");
		$g->add_vertex("c", "d");
	    }
	    for (1, 2) {
		ok(  $g->has_vertices( ) );
		ok(  $g->has_vertex("a") );
		ok(  $g->has_vertex($b ) );
		if ($g->hypervertexed) {
		    ok(  $g->has_vertex("c", "d") );
		}
		ok( !$g->has_vertex("e") );
	    }
	    for (1, 2) {
		is( $g->get_vertex_count("a"),      $m ? 2 : 1 );
		is( $g->get_vertex_count($b ),      $m ? 2 : 1 );
		if ($g->hypervertexed) {
		    is( $g->get_vertex_count("c", "d"), $m ? 2 : 1 );
		}
		is( $g->get_vertex_count("e"),      0 );
	    }
	    if (0) {
		is( $g->get_vertex_value("a"), "a" );
		is_deeply( $g->get_vertex_value($b), $b );
		if ($g->hypervertexed) {
		    is_deeply( $g->get_vertex_value("c", "d"), $h ? "d" : "c" ); # @todo
		}
		is( $g->get_vertex_value("e"), undef);
	    }
	}
    }
}

