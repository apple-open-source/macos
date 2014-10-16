#!/usr/bin/perl -w

use Graph;
use Test::More;
plan tests => 4;

my $graph = Graph->new( undirected => 1 );

$graph->add_vertex("Berlin");
$graph->add_vertex("Bonn");
$graph->add_edge("Berlin","Bonn");
is ("$graph","Berlin=Bonn");
$graph->set_edge_attributes("Berlin", "Bonn", { color => "red" });
is ("$graph","Berlin=Bonn");

$graph = Graph->new( undirected => 1 );

$graph->add_vertex("Berlin");
$graph->add_vertex("Bonn");
$graph->add_edge("Bonn","Berlin");
is ("$graph","Berlin=Bonn");
$graph->set_edge_attributes("Bonn", "Berlin", { color => "red" });
is ("$graph","Berlin=Bonn");


