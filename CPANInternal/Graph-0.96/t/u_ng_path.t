use Test::More tests => 1196;

 # http://rt.cpan.org/NoAuth/Bug.html?id=1179

=head1 NAME

Test program for Graph.

=head2 SYNOPSIS

   perl test_graph.pl [size (default 3 works well)]

=head2 DESCRIPTION

This program constructs size x size "square" directed and undirected
graphs, then tests various path-finding related methods:
TransitiveClosure_Floyd_Warshall, APSP_Floyd_Warshall, and SSSP (all
flavors).

You can think of each node as a cell in a matrix. In the directed
case, each node is has edges from its neighbors down and right (the
coordinates growing to down and right), eg, node 1,1 is has edges to
nodes 1,2, node 2,1, and node 2,2.  In the undirected case, the down
left diagonal is present as well, eg, from node 1,1 to node 0,2.  All
edges have unit weight.

This structure makes it easy to calculate the correct answers.  For
example, the all-pairs-shortest-path in the directed case should have
an edge from every node to every other node that is further down or to
the right, and the weight should be equal to the maximun difference in
the coordinates of the nodes.  Eg, the weight from node 1,1 to node
3,3 is 2 along the path 1,1 to 2,2 to 3,3.

=head1 AUTHOR

Nathan Goodman

=cut

use Graph;
use Graph::Directed;
use Graph::Undirected;

# set up square graphs
for my $size (0..3) {
    print "# size = $size\n";
    $g=Graph::Directed->new(compat02 => 1);
    $h=Graph::Undirected->new(compat02 => 1);
    $g=construct($g, $size);
    $h=construct($h, $size);
    test_graph($g, $size);
    test_graph($h, $size);
    test_tc($g, $size);
    test_tc($h, $size);
    test_apsp($g, $size);
    test_apsp($h, $size);
    #test_sssp($g,$size,'Dijkstra');
    #test_sssp($h,$size,'Dijkstra');
    #test_sssp($g,'Bellman_Ford');
    #test_sssp($h,'Bellman_Ford');
    #test_sssp($g,'DAG');
}

exit;

sub construct {
  my($g, $size)=@_;
  for (my $i=0;$i<$size;$i++) {
    for (my $j=0;$j<$size;$j++) {
      my $node=node($i,$j);
      $g->add_vertex($node);
      $g->add_weighted_edge(node($i-1,$j),1,$node) if $i>0;
      $g->add_weighted_edge(node($i,$j-1),1,$node) if $j>0;
      $g->add_weighted_edge(node($i-1,$j-1),1,$node) if $i>0 && $j>0; # down-right diagonal
      $g->add_weighted_edge(node($i-1,$j+1),1,$node) 
	if $g->undirected && $i>0 && $j<$size; # down-left diagonal
    }
  }
  return $g;
}

# check graph construction
# all nodes that are distance 1 apart in the rectangle 
#   should be connected by an edge of weight 1 in the graphs
sub test_graph {
  my($g, $size)=@_;
  print "# test_graph ",ref $g,"\n";
   for (my $i1=0;$i1<$size;$i1++) {
    for (my $j1=0;$j1<$size;$j1++) {
      my $node1=node($i1,$j1);
      for (my $i2=0;$i2<$size;$i2++) {
	for (my $j2=0;$j2<$size;$j2++) {
	  my $node2=node($i2,$j2);
	  if (dist($g,$node1,$node2)==1) {
	    ok($g->has_edge($node1,$node2), "edge $node1-$node2");
	    my $weight=weight($g,$node1,$node2);
	    is($weight, 1, "edge weight on edge $node1-$node2");
	  } else {
	    ok(!$g->has_edge($node1, $node2), "extra edge $node1-$node2");
	  }}}}}}

# check transitive closure
# all nodes that are distance 0 or more apart in the rectangle 
#   should be connected by an edge in the grapsh -- weights not used
sub test_tc {
  my($g, $size)=@_;
  print "# test_tc ",ref $g,"\n";
  my $gt=$g->TransitiveClosure_Floyd_Warshall;
   for (my $i1=0;$i1<$size;$i1++) {
    for (my $j1=0;$j1<$size;$j1++) {
      my $node1=node($i1,$j1);
      for (my $i2=0;$i2<$size;$i2++) {
	for (my $j2=0;$j2<$size;$j2++) {
	  my $node2=node($i2,$j2);
	  if (dist($gt,$node1,$node2)>=0) {
	      ok(  $gt->has_edge($node1,$node2), "edge $node1-$node2");
	  } else {
	      ok( !$gt->has_edge($node1,$node2), "extra edge $node1-$node2");
	  }}}}}}

# check all pairs shortest path
# all nodes that are distance 0 or more apart in the rectangle 
#   should be connected by an edge in the graph with weight equal to distance
sub test_apsp {
  my($g, $size)=@_;
  print "# test_apsp ",ref $g,"\n";
  my $gs=$g->APSP_Floyd_Warshall;
   for (my $i1=0;$i1<$size;$i1++) {
    for (my $j1=0;$j1<$size;$j1++) {
      my $node1=node($i1,$j1);
      for (my $i2=0;$i2<$size;$i2++) {
	for (my $j2=0;$j2<$size;$j2++) {
	  my $node2=node($i2,$j2);
	  my $dist=dist($gs,$node1,$node2);
	  if ($dist>=0) {
	    ok($gs->has_edge($node1,$node2), "edge $node1-$node2");
	    my $weight=weight($gs,$node1,$node2);
	    is( $weight, $dist, "edge weight $node1-$node2" );
	    test_path($gs,$node1,$node2,path($gs,$node1,$node2),weight($gs,$node1,$node2));
	  } else {
	    ok( !$gs->has_edge($node1,$node2),
		"extra edge $node1-$node2" );
	  }}}}}}

# check single source shortest path
# all nodes that are distance 0 or more apart in the rectangle 
#   should be connected by an edge in the graph with weight equal to distance
sub test_sssp {
  my($g,$size,$alg)=@_;
  print "# test_sssp $alg ",ref $g,"\n";
  my $sssp=$g->can("SSSP_$alg");
  for (my $i1=0;$i1<$size;$i1++) {
    for (my $j1=0;$j1<$size;$j1++) {
      my $node1=node($i1,$j1);
      print "# --- source $node1\n";
      my $gs=$g->$sssp($node1);
      for (my $i2=0;$i2<$size;$i2++) {
	for (my $j2=0;$j2<$size;$j2++) {
	  my $node2=node($i2,$j2);
	  my $dist=dist($g,$node1,$node2);
	  my $weight=weight($gs,$node2);
	  my $path=path($gs,$node2);
	  test_path($gs,$node1,$node2,$path,$weight);
	  if ($dist>0) {
	    ok( $weight > 0, "path weight $node1-$node2" );
	    is( $weight, $dist, "path weight $node1-$node2" );
	  } else {
	      is( $weight, 0, "path weight $node1-$node2" );
	  }}}}}}

sub test_path {
  my($g,$s,$t,$path,$weight)=@_;
  return if $s eq $t;
  is( @$path - 1, $weight, "path $s-$t length does not equal weight");
  if ($weight) {
      # path may be reversed
      my @path=@$path;
      @path=reverse @path if $path[$#path] eq $s;
      is( $path[0], $s, "path $s-$t should start at $s");
      for (my $i=0;$i<$#path;$i++) {
	  is(dist($g,$path->[$i],$path->[$i+1]), 1,
	     "adjacent nodes in path $s-$t should be distance 1 apart");
      }
  }
}

sub node {
  my($i,$j)=@_;
  $j=$i unless defined $j;
  "$i/$j";
}

sub weight {
  my($g,$node1,$node2)=@_;
  return $g->path_length($node1,$node2);
}

sub path {
  my($g,$node1,$node2)=@_;
  return [ $g->path_vertices($node1,$node2) ];
}

#distances in rectangular graphs with diagonal edges
sub dist {
  my($g)=shift @_;
  _dist($g->directed,@_);
}
  
sub _dist {
  my($directed)=shift @_;
  my($i1,$j1,$i2,$j2);
  if (@_==2) {
    # args are nodes
    my($node1,$node2)=@_;
    ($i1,$j1)=split('/',$node1);
    ($i2,$j2)=split('/',$node2);
  } else {
    # args are indices
    ($i1,$j1,$i2,$j2)=@_;
  }
  if ($directed) {
    return -1 unless $i1<=$i2 && $j1<=$j2;
    return max($i2-$i1,$j2-$j1);
  } else {
    return max(abs $i2-$i1,abs $j2-$j1);
  }
}

sub min {
  if ($#_==0) {@_=@{$_[0]} if 'ARRAY' eq ref $_[0];}
  return undef unless @_;
  if ($#_==1) {my($x,$y)=@_; return ($x<=$y?$x:$y);}
  my $min=shift @_;
  map {$min=$_ if $_<$min} @_;
  $min;
}

sub max {
  if ($#_==0) {@_=@{$_[0]} if 'ARRAY' eq ref $_[0];}
  return undef unless @_;
  if ($#_==1) {my($x,$y)=@_; return ($x>=$y?$x:$y);}
  my $max=shift @_;
  map {$max=$_ if $_>$max} @_;
  $max;
}
