# rt.cpan.org #20185: problem with SPT_Bellman_Ford

use strict;

use Test::More tests => 7;

use Graph;
use Graph::Directed;
use Graph::Undirected;

my $g_1 = Graph::Undirected -> new(unionfind => 1);

my @edge =
    (
     [ '16977', '14903' ],
     [ '21062',  '4504' ],
     [ '14671', '10554' ],
     [ '14903',  '8891' ],
     [  '9714', '14671' ],
     [  '4504', '13544' ],
     [  '9714', '13544' ],
     [ '16977',  '8891' ],
     [ '21062', '21062' ],
     [  '9714',  '4504' ],
     [ '14671', '21687' ],
     [ '14671', '16977' ],
     [  '4504', '21687' ],
     [ '10554', '14903' ],
     [  '9714', '21687' ],
     [ '13544', '14671' ],
     [ '21062', '14671' ],
     [ '10554',  '8891' ],
     [ '14671', '14903' ],
     [ '14671', '14671' ],
     [ '13544', '13544' ],
     [ '14671', '14026' ],
     [  '4504', '14671' ],
     [ '14671',  '8891' ],
     [ '13544', '14026' ],
     [ '10554', '16977' ],
    );
	
$g_1 -> add_edges(@edge);

my $spt_1 = $g_1 -> SPT_Bellman_Ford;

is($spt_1->vertices, $g_1->vertices);

my $g_2 = Graph::Undirected -> new();

$g_2 -> add_edges(@edge);

my $spt_2 = $g_2 -> SPT_Bellman_Ford;

is($spt_2->vertices, $g_2->vertices);

my $g_3 = Graph::Directed -> new();

$g_3 -> add_edges(@edge);

my $spt_3a = $g_3 -> SPT_Bellman_Ford('21062');

is($spt_3a->vertices, $g_3->vertices - 1);
ok(!$spt_3a->has_vertex('9714'));

my $spt_3b = $g_3 -> SPT_Bellman_Ford('4504');

is($spt_3b->vertices, $g_3->vertices - 2);
ok(!$spt_3b->has_vertex('9714'));
ok(!$spt_3b->has_vertex('21062'));
