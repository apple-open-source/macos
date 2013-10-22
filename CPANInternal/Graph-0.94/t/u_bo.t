use Test::More tests => 95;

use strict;
use Graph;

use lib "t";
require "simple.pl";

sub rt_17159 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1, $v2, $v3, $v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    for my $v ($v1, $v2, $v3, $v4) {
	rt_17159_check($v);
    }
    my @ap = $g->articulation_points;
    for my $ap (@ap) {
	rt_17159_check($ap);
    }
    sub rt_17159_check {
	my $z = shift;
	ok($z->xyz());
    }
}

rt_17159(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17159(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17159(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

sub rt_17160 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1, $v2, $v3, $v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    for my $v ($v1, $v2, $v3, $v4) {
	rt_17160_check($v);
    }
    my @cc = $g->connected_components;
    for my $ref (@cc) {
	for (@$ref) {
	    rt_17160_check($_);
	}
    }
    sub rt_17160_check {
	my $z = shift;
	ok($z->xyz());
    }
}

rt_17160(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17160(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17160(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

sub rt_17161 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1, $v2, $v3, $v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    for my $v ($v1, $v2, $v3, $v4) {
	rt_17161_check($v);
    }
    my @b = $g->bridges;
    for my $ref (@b) {
	for (@$ref) {
	    rt_17161_check($_);
	}
    }
    sub rt_17161_check {
	my $z = shift;
	ok($z->xyz());
    }
}

rt_17160(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17160(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17160(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

sub rt_17162 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1, $v2, $v3, $v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    for my $v ($v1, $v2, $v3, $v4) {
	rt_17162_check($v);
    }
    my $cl = ref $v1;
    my $cg = $g->connected_graph(super_component => sub {
				     $cl->new();
				 });
    my @cv = $cg->vertices;
    for my $ref (@cv) {
	rt_17162_check($ref);
    }
    sub rt_17162_check {
	my $z = shift;
	ok($z->xyz());
    }
}

rt_17162(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17162(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17162(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

sub rt_17163 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1,$v2,$v3,$v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    my @spd = $g->SP_Dijkstra($v1,$v4);
    ok(@spd >= 2);
}

rt_17163(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17163(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17163(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

sub rt_17164 {
    my $g = Graph::Undirected->new;
    my ($v1, $v2, $v3, $v4) = @_;
    $g->add_vertices($v1,$v2,$v3,$v4);
    $g->add_edges([$v1,$v2],[$v3,$v4],[$v3,$v2]);
    my @spbf = $g->SP_Bellman_Ford($v1,$v4);
    ok(@spbf >= 2);
    is($spbf[ 0], $v1);
    is($spbf[-1], $v4);
}

rt_17164(Foo->new(),
	 Foo->new(),
	 Foo->new(),
	 Foo->new());

rt_17164(Bar->new(1),
	 Bar->new(2),
	 Bar->new(3),
	 Bar->new(4));

rt_17164(Bar->new(),
	 Bar->new(),
	 Bar->new(),
	 Bar->new());

{
    # rt.cpan.org: 17592: articulation_points doesn't find all vertices

    my $g = Graph::Undirected->new;

    my $v1 = Foo->new();
    my $v2 = Foo->new();
    my $v3 = Foo->new();
    my $v4 = Foo->new();
    my $v5 = Foo->new();
    my $v6 = Foo->new();
    my $v7 = Foo->new();

    $g->add_vertices($v1,$v2,$v3,$v4,$v5,$v6,$v7);

    $g->add_edges([$v1,$v2],[$v2,$v3],[$v3,$v4],
		  [$v5,$v6],[$v6,$v7]);

    my @rts = $g->articulation_points;
    my %rts; @rts{@rts} = @rts;

    is(@rts, 3);
    ok($rts{$v2});
    ok($rts{$v3});
    ok($rts{$v6});
}
