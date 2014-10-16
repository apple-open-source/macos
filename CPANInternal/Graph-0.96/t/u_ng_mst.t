use Test::More qw/no_plan/;

=head1 NAME

Test program for Graph.

=head2 SYNOPSIS

   perl u_ng_mst.t [ A [ D [ N ] ] ]

=head2 DESCRIPTION

This program constructs various trees, embeds them in general graphs,
and tests various minimum spanning tree methods: MST_Kruskal,
MST_Prim, MST_Dijkstra.

A is arity and it defaults to 4.
D is depth and it defaults to 3.
N is chain/star size and it defaults to 40.  (The minimum is 10.)
(To use a default, specify '-'.)

=head1 AUTHOR

Nathan Goodman

=cut

my ($A, $D, $N) = @ARGV;

$A = 3  if ($A || 0) < 1;
$D = 4  if ($D || 0) < 1;
$N = 40 if ($N || 0) < 1;

use strict;
use Graph;
use Graph::Directed;
use Graph::Undirected;

for my $arity (1..$A) {
  for my $depth (1..$D) {
    print "# depth=$depth, arity=$arity\n";
    #  $g=construct(new Graph::Directed,$depth,$arity);
    my $h=construct(new Graph::Undirected,$depth,$arity);
    my $t=regular_tree(new Graph::Undirected,$depth,$arity);
    my $mst1=$h->MST_Kruskal;
    is($mst1,$t,"Kruskal");
    my $mst2=$h->MST_Prim;
    is($mst2,$t,"Prim");
    my $mst3=$h->MST_Dijkstra;
    is($mst3,$t,"Dijkstra");
    #  ok(1,"end of tests for depth=$depth, arity=$arity");
  }
}
# do some long chains 
my $arity=1;
for(my $depth=10;$depth<=$N;$depth+=10) {
  print "# depth=$depth, arity=$arity\n";
  #  $g=construct(new Graph::Directed,$depth,$arity);
  my $h=construct(new Graph::Undirected,$depth,$arity);
  my $t=regular_tree(new Graph::Undirected,$depth,$arity);
  my $mst1=$h->MST_Kruskal;
  is($mst1,$t,"Kruskal");
  my $mst2=$h->MST_Prim;
  is($mst2,$t,"Prim");
  my $mst3=$h->MST_Dijkstra;
  is($mst3,$t,"Dijkstra");
  #  ok(1,"end of tests for depth=$depth, arity=$arity");
}
# do some wide stars
my $depth=1;
for(my $arity=10;$arity<=$N;$arity+=10) {
  print "# depth=$depth, arity=$arity\n";
  #  $g=construct(new Graph::Directed,$depth,$arity);
  my $h=construct(new Graph::Undirected,$depth,$arity);
  my $t=regular_tree(new Graph::Undirected,$depth,$arity);
  my $mst1=$h->MST_Kruskal;
  is($mst1,$t,"Kruskal");
  my $mst2=$h->MST_Prim;
  is($mst2,$t,"Prim");
  my $mst3=$h->MST_Dijkstra;
  is($mst3,$t,"Dijkstra");
  #  ok(1,"end of tests for depth=$depth, arity=$arity");
}

exit;

sub construct {
  my($g, $depth, $arity, $density)=@_;
  $density or $density=3;

  # make a tree with edge weights of1
  $g=regular_tree($g,$depth,$arity);
  # add heavier edges
  my @nodes=$g->vertices;
  my $new_edges=int $density*@nodes;
  for (1..$new_edges) {
    my $i=int rand $#nodes;
    my $j=int rand $#nodes;
    next if $g->has_edge($nodes[$i],$nodes[$j]);
    $g->add_weighted_edge($nodes[$i],$nodes[$j],2);
  }
  print "# V = ", scalar $g->vertices, ", E = ", scalar $g->edges, "\n";
  return $g;
}

sub regular_tree {
  my($tree,$depth,$arity,$root)=@_;
  defined $root or do {
    $root=0;
    $tree->add_vertex($root);
  };
  if ($depth>0) {
    for (my $i=0; $i<$arity; $i++) {
      my $child="$root/$i";
      $tree->add_vertex($child);
      $tree->add_weighted_edge($root,$child,1);
      regular_tree($tree,$depth-1,$arity,$child);
    }
  }
  $tree;
}

sub is_quiet {
  my($a,$b,$tag)=@_;
  return if $a eq $b;
  is($a,$b,$tag);
}
sub ok_quiet {
  my($bool,$tag)=@_;
  return if $bool;
  ok($bool,$tag);
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
