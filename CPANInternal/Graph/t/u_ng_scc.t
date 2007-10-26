use strict;
use Carp;
use Getopt::Long;
use Text::Abbrev;
use Graph::Directed;
use Test::More qw(no_plan); # random graphs

my $cmd_line="$0 @ARGV";
my %OPTIONS;

GetOptions (\%OPTIONS, 
	    qw(help verbose echo|X seed=i
	       input cycle cone grid random
               circumference=i 
               height=i width=i
               nodes=i edges=i density=f
	      ));
$OPTIONS{help} and die <<USAGE

Usage: $0 [options] [<input file>] [<output file>]

Test StrongComponents algorithm. 

The test-graphs can be any of the following:

  1. Provided as input in a file or STDIN
     Expressed as pairs (node1 node2) one pair per line

  2. Cycle.
     Size is specified by --circumference
     Density of cross edges specified by --density

  3. Cone (layered cycles of increasing size)
     --circumference is size of largest cycle
     
  4. Grid. 
     Size is specified by --height and --width options. If just 1
     number is specified, the grid is square. If neither is specified
     defaults are used.

  5. Random graph.
     Size is specifed by any two of the following:
       number of nodes (--nodes option)
       number of edges (--edges)
       density (edges/nodes, --density option)
    If 0 or 1 are specified, defaults are used for rest.

    The size is a target. The actual graph may be a little smaller due
    to duplicate edges.

Default is --grid and --random

Options
-------
  --help              Print this message
  --verbose           (for testing)
  -X or --echo        Echo command line (for testing and use in scripts)
  --seed              Seed for random number generator
  --input             Graph is provided as input
  --cycle             Generate cycle graph
  --cone              Generate cone graph
  --circumference     Numbre of nodes in cycle graph
  --grid              Generate grid (implied by --height or --width)
  --height            Grid height
  --width             Grid width
  --random            Generate random graph (implied by --nodes, --edges)
  --nodes             Number of nodes in random graph
  --edges             Number of edges in random graph
  --density           Average number of cross edges per node in cycle graph
                      Average number of edges per node in random graph

Options and values may be abbreviated.  Values are case insensitive.

USAGE
;
print OUT "$cmd_line\n" if $OPTIONS{echo};
srand $OPTIONS{seed} if defined $OPTIONS{seed};

my %DEFAULTS=
  (circumference=>10,
   height=>3,
   width=>3,
   nodes=>10,
   edges=>11,
   density=>1.1,
  );
my $INPUT=$OPTIONS{input} || @ARGV>=2;
my $CYCLE=$OPTIONS{cycle};
my $CONE=$OPTIONS{cone};
$CYCLE || $CONE or ($OPTIONS{circumference} and $CYCLE=1);
my $GRID=$OPTIONS{grid} || $OPTIONS{height} || $OPTIONS{width};
my $RANDOM=$OPTIONS{random} || $OPTIONS{nodes} || $OPTIONS{edges};
$CYCLE=$CONE=$GRID=$RANDOM=1 unless $INPUT || $CYCLE || $CONE || $GRID || $RANDOM;

my $CIRCUMFERENCE=get_param('circumference') if $CYCLE;
my $CONE_SIZE=get_param('circumference') if $CONE;
my($HEIGHT,$WIDTH)=grid_size() if $GRID;
my($NODES,$EDGES)=random_size() if $RANDOM;
my $DENSITY=get_param('density');

if ($INPUT && @ARGV) {
  my  $input_file=shift @ARGV;
  open(IN,$input_file) || confess "Cannot open input file $input_file: $!";
} else {
  *IN=*STDIN;
}
if (@ARGV) {
  my $output_file=shift @ARGV;
  open(OUT,"> $output_file") || die "Cannot create output file $output_file: $!";
} else {
  *OUT=*STDOUT;
}
if ($INPUT) {
  print"# Test input graph\n";
  my $graph=input_graph();
  test_sc($graph);
}
if ($CYCLE) {
  print"# Test cycle graph\n";
  my $graph=cycle_graph();
  my $components=test_sc($graph);
  is(@$components, 1, "number of components");
}
if ($CONE) {
  print"# Test cone graph\n";
  my $graph=cone_graph();
  my $components=test_sc($graph);
  test_cone_sc($graph,$components);

  print"# Test cone graph with intermediate nodes\n";
  my $graph2=cone_graph('intermediate');
  my $components2=test_sc($graph2);
  test_coneint_sc($graph2,$components2);
}
if ($GRID) {
  print"# Test grid graph (acyclic)\n";
  my $graph=grid_graph();
  my $nodes=$graph->vertices;
  my $components=test_sc($graph);
  is(@$components, $nodes);
  
  print"# grid graph (cyclic)\n";
  my $graph2=grid_graph('cyclic');
  my $components2=test_sc($graph2);
  is(@$components2, 1, "number of components");
}
if ($RANDOM) {
  print"# Test random graph (acyclic)\n";
  my $graph=random_graph();
  my $components=test_sc($graph);
  my $nodes=$graph->vertices;
  is(@$components, $nodes);

  print"# Test random graph (cyclic)\n";
  my $graph2=random_graph('cyclic');
  my $components2=test_sc($graph2);
}

sub input_graph {
  my $graph=new Graph::Directed;
  while (<IN>) {
    my($node0,$node1)=split;
    next unless length($node0) && length($node1);
    $graph->add_weighted_edge($node0,$node1,1);
  }
  $graph;
}
sub cycle_graph {
  my($circumference,$density)=@_;
  defined $circumference or $circumference=$CIRCUMFERENCE;
  defined $density or $density=$DENSITY;
  my $graph=new Graph::Directed;
  # make simple cycle
  for (my $i=1; $i<$circumference; $i++) {
    $graph->add_weighted_edge($i-1,$i,1);
  }
  $graph->add_weighted_edge($circumference-1,0,1);
  # add random cross edges
  my $cross_edges=int $density*$circumference;
  for (my $i=0;$i<$cross_edges; $i++) {
    my($node1,$node2)=(int rand $circumference,int rand $circumference);
    $graph->add_weighted_edge($node1,$node2,1);
  }
  $graph;
}
sub cone_graph {
  my($intermediate)=@_;
  my $graph=new Graph::Directed;
  print"# Constructing cone graph...\n";
  # make $CONE_SIZE simple cycles of sizes 1..$CONE_SIZE
  for (my $i=0; $i<$CONE_SIZE; $i++) {
    my $circumference=$i+1;
    # make simple cycle
    for (my $j=1; $j<$circumference; $j++) {
      $graph->add_weighted_edge($i.'/'.($j-1),"$i/$j",1);
    }
    $graph->add_weighted_edge($i.'/'.($circumference-1),"$i/0",1);
    # add random cross edges
    my $cross_edges=int $DENSITY*$i;
    for (my $j=0;$j<$cross_edges; $j++) {
      my($j1,$j2)=(int rand $circumference,int rand $circumference);
      $graph->add_weighted_edge("$i/$j1","$i/$j2",1);
    }
  }
  # add random edges between cycles
  my $nodes=0+$graph->vertices;
  my $edges=int $DENSITY*$nodes;
  for (my $i=0;$i<$edges; $i++) {
    my($i1,$i2)=(int rand $CONE_SIZE,int rand $CONE_SIZE);
    ($i1,$i2)=sort2($i1,$i2);
    my($j1,$j2)=(int rand $i1+1,int rand $i2+1);
    if ($intermediate && $i1 != $i2) {
      $graph->add_weighted_edge("$i1/$j1","i$i",1);
      $graph->add_weighted_edge("i$i","$i2/$j2",1);
    } else {
      $graph->add_weighted_edge("$i1/$j1","$i2/$j2",1);
    } 
  }
  $graph;
}

sub grid_graph {
  my($cyclic)=@_;
  my $graph=new Graph::Directed;
  for (my $i=0; $i<$HEIGHT; $i++) {
    for (my $j=0; $j<$WIDTH; $j++) {
      my $node=grid_node($i,$j);
      $graph->add_vertex($node);
      $graph->add_weighted_edge(grid_node($i-1,$j),$node,1) if $i>0; # down
      $graph->add_weighted_edge(grid_node($i,$j-1),$node,1) if $j>0; # right
      $graph->add_weighted_edge(grid_node($i-1,$j-1),$node,1)        # down-right diagonal
	if !$cyclic && $i>0 && $j>0;
#      $graph->add_weighted_edge(grid_node($i-1,$j+1),$node,1)        # down-left diagonal
#	if $cyclic && $i>0 && $j<$WIDTH-1;
    }
  }
  if ($cyclic) {		# add wrapround edges, making grid a torus
    if ($WIDTH>1) {
      for (my $i=0; $i<$HEIGHT; $i++) {
	$graph->add_weighted_edge(grid_node($i,$WIDTH-1),grid_node($i,0),1);
      }
    }
    if ($HEIGHT>1) {
      for (my $j=0; $j<$WIDTH; $j++) {
	$graph->add_weighted_edge(grid_node($HEIGHT-1,$j),grid_node(0,$j),1);
      }
    }
  }
  $graph;
}
sub random_graph {
  my($cyclic)=@_;
  my $graph=new Graph::Directed;
  for (my $i=0;$i<$EDGES; $i++) {
    my($node1,$node2)=(int rand $NODES,int rand $NODES);
    ($node1,$node2)=sort2($node1,$node2) unless $cyclic;
    $graph->add_weighted_edge($node1,$node2,1) unless $node1==$node2;
  }
  $graph;
} 

sub get_param {
  my($keyword)=@_;
  $keyword=lc $keyword;
  my $param=$OPTIONS{$keyword};
  defined $param or $param=$DEFAULTS{$keyword};
  $param;
}

sub grid_size {
  my $height=$OPTIONS{height};
  my $width=$OPTIONS{width};
  # Cases
  # 1. no height && no width => use defaults
  # 2. height && no width    => set width=height
  # 3. no height && width    => set height=width
  # 4. height && width       => use as given
  if (!defined $height && !defined $width) {
    $height=$DEFAULTS{height};
    $width=$DEFAULTS{width};
  } elsif (defined $height && !defined $width) {
    $width=$height;
  } elsif (!defined $height && defined $width) {
    $height=$width;
  }
  ($height,$width);
}
sub grid_node {
  my($i,$j)=@_;
  $j=$i unless defined $j;
  "$i/$j";
}
#distances in directed grid with either diagonal edges or wraparound edges
sub grid_dist {
  my($node1,$node2,$cyclic)=@_;
  my($i1,$j1)=split('/',$node1);
  my($i2,$j2)=split('/',$node2);
  if ($cyclic) {		# wraparound edges
    return ($i2-$i1)%$HEIGHT + ($j2-$j1)%$WIDTH;

  } else {
    return undef unless $i1<=$i2 && $j1<=$j2;
    return max($i2-$i1,$j2-$j1);
  }
}
sub random_size {
  my $nodes=$OPTIONS{nodes};
  my $edges=$OPTIONS{edges};
  my $density=$OPTIONS{density};
  # Cases
  # nodes  edges  density  action
  # no     no     no       use default nodes, edges
  # no     no     yes      use default nodes
  # no     yes    no       use default density
  # no     yes    yes      okay
  # yes    no     no       use default density
  # yes    no     yes      okay
  # yes    yes    no       okay
  # yes    yes    yes      overspecified -- use nodes, edges
  if (!defined $nodes) {
    if (!defined $edges && !defined $density) {
      ($nodes,$edges)=@DEFAULTS{qw(nodes edges)};
    } elsif (!defined $edges && defined $density) {
      $nodes=$DEFAULTS{nodes};
      $edges=int $nodes*$density;
    } elsif (defined $edges && !defined $density) {
      $nodes=int ($edges/$DEFAULTS{density});
    } else {
      $nodes=int ($edges/$density);
    }
  } else {
    if (!defined $edges && !defined $density) {
      $edges=int $nodes*$DEFAULTS{density};
    } elsif (!defined $edges && defined $density) {
      $edges=int $nodes*$density;
    } 
    # remaining cases have $edges set, so nothing to do
  }
  ($nodes,$edges);
}

# check strong components
sub test_sc {
  my($graph)=@_;
  print OUT "+++ test_sc\n" if $OPTIONS{verbose};
  my @components=$graph->strongly_connected_components;
  my $tc=new Graph::TransitiveClosure($graph);
  check_partition($graph,@components);	# check that components partition nodes
  check_reach($graph,$tc,@components);         # check reachability of components 
  \@components;
}
sub check_partition {
  my($graph,@components)=@_;
  my @nodes=$graph->vertices;
  my %hits;
  for my $component (@components) {
    map {$hits{$_}++} @$component;
  }
  for my $node (@nodes) {
    is($hits{$node}, 1, "node $node");
  }
}
sub check_reach {
  my($graph,$tc,@components)=@_;
  # for each (ordered) pair of nodes in each component, make sure 1st node can reach 2nd
  for my $component (@components) {
    for (my $i=0; $i<@$component; $i++) {
      my $nodei=$component->[$i];
      for (my $j=0; $j<@$component; $j++) {
	my $nodej=$component->[$j];
	ok($tc->is_reachable($nodei,$nodej),
	   "nodes $nodei, $nodej in same component and reachable");
      }
    }
  }
  # for each pair of mutually reachable nodes, make sure they're in same component
  my %components;
  for my $component (@components) { # put each node in its component
    @components{@$component}=($component)x@$component;
  }
  my @nodes=$graph->vertices;
  for (my $i=0; $i<@nodes; $i++) {
    my $nodei=$nodes[$i];
    for (my $j=$i; $j<@nodes; $j++) {
      my $nodej=$nodes[$j];
      next unless $tc->is_reachable($nodei,$nodej) && $tc->is_reachable($nodej,$nodei);
      is($components{$nodei}, $components{$nodej},
         "nodes $nodei, $nodej reachable and in same component");
    }
  }
}

# check strong components of cone graph
# exploits the special structure of cycle, namely, cycles are in layers
sub test_cone_sc {
  my($graph,$components)=@_;
  is(@$components, $CONE_SIZE);
  # each component should be different size
  my %size2component;
  for my $component (@$components) {
    my $size=@$component;
    ok(!$size2component{$size}, "two components have same size ($size)");
    $size2component{$size}=$component;
  }
  # check that each component corresponds to a layer
  for (my $i=0; $i<$CONE_SIZE; $i++) {
    my $size=$i+1;
    my $component=$size2component{$size};
    # ecah node should start with "$i/"
    my @bad=grep {$_!~/^$i\//} @$component;
    is(@bad, 0, "component $size does not contain wrong nodes (@bad)");
  }
}
# check strong components of cone graph with intermediate nodes
# exploits the special structure of cycle, namely, cycles are in layers
sub test_coneint_sc {
  my($graph,$components)=@_;
  my @nodes=$graph->vertices;
  my @intermediates=grep {$_=~/^i/} @nodes;
  is(@$components, $CONE_SIZE+@intermediates);
  # each intermediate node should be in own component
  for my $component (@$components) {
    my @ints=grep {$_=~/^i/} @$component;
    ok(@ints <= 1, "component contains multiple intermediate nodes @ints");
    ok(!(@ints==1 && @$component>1), "component contains cycle and intermediate nodes");
  }
  # each component should be different size unless it contains just an intermediate node
  my %size2component;
  my %intermediates;
  for my $component (@$components) {
    my $size=@$component;
    next if grep {$_=~/^i/} @$component;
    ok(!($size2component{$size}), "two components have same size ($size)");
    $size2component{$size}=$component;
  }
  # check that each non-intermediate node component corresponds to a layer 
  for (my $i=0; $i<$CONE_SIZE; $i++) {
    my $size=$i+1;
    my $component=$size2component{$size};
    # each node should start with "$i/"
    my @bad=grep {$_!~/^$i\//} @$component;
    is(@bad, 0, "component $size contains wrong nodes @bad");
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
sub mindef {min(grep {defined $_} @_);}
sub maxdef {max(grep {defined $_} @_);}
sub sort2 {$_[0]<=$_[1]? ($_[0],$_[1]): ($_[1],$_[0]);}
