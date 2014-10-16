use Test::More tests => 272;

use Graph::Directed;
use Graph::Undirected;
use Graph::Traversal::DFS;

my $g0 = Graph::Undirected->new;
my $g1 = Graph::Directed->new;
my $g2 = Graph::Undirected->new; # cyclic
my $g3 = Graph::Undirected->new; # unconnected
my $g4 = Graph::Directed->new;   # cyclic loop
my $g5 = Graph::Directed->new;   # cyclic
my $g6 = Graph::Directed->new;
my $g7 = Graph::Undirected->new; # empty
my $g8 = Graph::Undirected->new; # only vertices
my $g9 = Graph::Directed->new;
my $ga = Graph::Directed->new;

$g0->add_path(qw(a b c));
$g0->add_path(qw(a b d));
$g0->add_path(qw(a e f));

$g1->add_path(qw(a b c));
$g1->add_path(qw(a b d));
$g1->add_path(qw(a e f));

$g2->add_cycle(qw(a b c));

$g3->add_path(qw(a b c));
$g3->add_path(qw(d e f));

$g4->add_cycle(qw(a));

$g5->add_cycle(qw(a b c));

$g6->add_path(qw(a b c));
$g6->add_path(qw(d e f));

$g9->add_cycle(qw(a b c));
$g9->add_path(qw(b d e f));
$g9->add_edge(qw(d f));

$ga->add_cycle(qw(a b c));
$ga->add_path(qw(b d e f));
$ga->add_edge(qw(d f));

sub simple {
    my $g = shift;
    my @v = $g->vertices;
    is(@_, @v, "vertices");
    my %v; $v{$_} ++ for @_;
    # is(...,0) is 5.00504-incompatible
    ok(!scalar(grep { ($v{$_} || 0) != 1 } @v), "... once");
}

{
    my $t = Graph::Traversal::DFS->new($g0);

    is($t->unseen, $g0->vertices, "fresh traversal");
    is($t->seen,   0);
    is($t->seeing, 0);

    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g0, @t0);
    simple($g0, @t1);
    simple($g0, @t2);

    is($t->graph, $g0, "graph");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g0,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->postorder;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "a b c d e f", "pre");
    is("@post", "c d b f e a", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a", "roots");
    ok( $t->is_root('a') );
    ok(!$t->is_root('b') );
    ok(!$t->is_root('c') );
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g1,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1,
				       first_root     => 'b');
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "b c d a e f", "pre");
    is("@post", "c d b f e a", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}",  "b a", "roots");
    ok( $t->is_root('a') );
    ok( $t->is_root('b') );
    ok(!$t->is_root('c') );
}

{
    my $t0 = Graph::Traversal::DFS->new($g0, next_alphabetic => 1);
    is($t0->next, "a",   "scalar next");
    $t0->terminate;
    is($t0->next, undef, "terminate");
    $t0->reset;
    is($t0->next, "a",   "after reset scalar next");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g2,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g2, @t0);
    simple($g2, @t1);
    simple($g2, @t2);

    is("@pre",  "a b c",       "pre");
    is("@post", "c b a",       "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   3, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c", "seen all");
    is("@{[$t->roots]}", "a", "roots");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g3,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g3, @t0);
    simple($g3, @t1);
    simple($g3, @t2);

    is("@pre",  "a b c d e f", "pre");
    is("@post", "c b a f e d", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a d", "roots");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g4,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1,
				       find_a_cycle => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    is("@pre",  "a",       "pre");
    is("@post", "a",       "post");
    is("@t0",   "a",       "t0");
    is("@t1",   "a",       "t1");
    is("@t2",   "a",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   1, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a", "seen all");
    is("@{[$t->roots]}", "a", "roots");
    is("@{$t->{state}->{a_cycle}}", "a", "cycle");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g5,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1,
				       find_a_cycle => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    is("@pre",  "a b c",   "pre");
    is("@post", "c b",     "post");
    is("@t0",   "a b c",   "t0");
    is("@t1",   "c b",     "t1");
    is("@t2",   "c b",     "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   3, "seen all");
    is($t->seeing, 1, "seeing one");
    is("@{[sort $t->seen]}", "a b c", "seen all");
    is("@{[$t->roots]}", "a", "roots");
    is("@{$t->{state}->{a_cycle}}", "b c a", "cycle");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g2,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1,
				       find_a_cycle => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    is("@pre",  "a b c",   "pre");
    is("@post", "c b",     "post");
    is("@t0",   "a b c",   "t0");
    is("@t1",   "c b",     "t1");
    is("@t2",   "c b",     "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   3, "seen all");
    is($t->seeing, 1, "seeing one");
    is("@{[sort $t->seen]}", "a b c", "seen all");
    is("@{[$t->roots]}", "a", "roots");
    is("@{$t->{state}->{a_cycle}}", "b c a", "cycle");
}

{
    my $g = Graph::Undirected->new;
    $g->add_path(qw(a b c d e));
    $g->add_path(qw(b f g));
    $g->add_cycle(qw(c h i));
    my @c = $g->find_a_cycle(next_alphabetic => 1);
    is(@c, 3, "find_a_cycle");
    is("@c", "h i c", "find_a_cycle");
}

{
    my $g = Graph::Directed->new;
    my $h = Graph::Undirected->new;

    $g->add_path(qw(a b c d e));
    $g->add_path(qw(b f g));
    $g->add_path(qw(c h i));

    ok($g->is_dag, "is_dag true for dag");

    $h->add_path(qw(a b c d e));
    $h->add_path(qw(b f g));
    $h->add_path(qw(c h i));

    ok(!$h->is_dag, "is_dag false for undirected");

    my @t = $g->topological_sort(next_alphabetic => 1);

    is(@t, 9, "topological_sort");
    is("@t", "a b f g c h i d e", "topological_sort");

    ok($g->is_dag, "directed acyclic is dag");

    $g->add_path(qw(i c));

    ok(!$g->is_dag, "directed cyclic is not dag");
}

{
    my $g = Graph::Undirected->new;

    ok(!$g->is_dag, "undirected is not dag");

    eval '$g->topological_sort';
    like($@, qr/^Graph::topological_sort: expected directed acyclic graph, got undirected, /, "topological_sort not for undirected");

    my $d = Graph::Directed->new;

    $d->add_cycle(qw(a b));

    eval '$d->toposort';
    like($@, qr/^Graph::topological_sort: expected directed acyclic graph, got cyclic, /, "topological_sort not for cyclic");
}

{
    ok( $g0->is_connected, "is_connected");
    eval '$g1->is_connected';
    like($@,
	qr/Graph::is_connected: expected undirected graph, got directed, /,
	"directed cannot be tested for connectedness/");
    ok( $g1->is_weakly_connected, "... directed is weakly connected");
    ok( $g2->is_connected, "... cyclic undirected" );
    ok(!$g3->is_connected, "... undirected unconnected");
    eval '$g4->is_connected';
    like($@,
	qr/Graph::is_connected: expected undirected graph, got directed, /,
	"... cyclic loop");
    ok( $g4->is_weakly_connected, "... cyclic loop weakly connected");
    eval '$g5->is_connected';
    like( $@,
	 qr/Graph::is_connected: expected undirected graph, got directed, /,
	"... cyclic directed");
    ok( $g5->is_weakly_connected, "... cyclic directed weakly connected");
    eval '$g6->is_connected';
    like($@,
         qr/Graph::is_connected: expected undirected graph, got directed, /,
         "... directed unconnected");
    ok(!$g6->is_weakly_connected, "... directed unconnected is not weakly connected");
}

{
    my $t = Graph::Traversal::DFS->new($g7);

    is($t->unseen, $g7->vertices, "empty graph");
    is($t->seen,   0);
    is($t->seeing, 0);

    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g7, @t0);
    simple($g7, @t1);
    simple($g7, @t2);
}

{
    $g8->add_vertices(qw(a b c d));

    my $t = Graph::Traversal::DFS->new($g8);

    is($t->unseen, $g8->vertices, "only vertices");
    is($t->seen,   0);
    is($t->seeing, 0);

    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    simple($g8, @t0);
    simple($g8, @t1);
    simple($g8, @t2);
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g3,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       first_root => "a",
				       next_root  => undef);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    is("@pre",  "a b c", "pre");
    is("@post", "c b a", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 3, "unseen half");
    is($t->seen,   3, "seen half");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c", "seen half");
    is("@{[$t->roots]}", "a", "roots");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g3,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       start => "a");

    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->dfs;

    is("@pre",  "a b c", "pre");
    is("@post", "c b a", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 3, "unseen half");
    is($t->seen,   3, "seen half");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c", "seen half");
    is("@{[$t->roots]}", "a", "roots");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g0,
				       pre_edge  => sub { push @pre,  $_[0], $_[1] },
				       post_edge => sub { push @post, $_[0], $_[1] },
				       next_alphabetic => 1);

    $t->dfs;

    is("@pre",  "a b b c b d a e e f", "pre");
    is("@post", "b c b d a b e f a e", "post");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a", "roots");
}

my $gb = Graph->new;

$gb->add_cycle(qw(a b c));
$gb->add_path(qw(a c));
$gb->add_path(qw(a d b));
my @gb;
my $tb =	
    Graph::Traversal::DFS->
    new($gb,
	next_alphabetic => 1,
	pre_edge      => sub { push @gb, "pre_edge @_[0,1]" },
	post_edge     => sub { push @gb, "post_edge @_[0,1]" },
	non_tree_edge => sub { push @gb, "non_tree_edge @_[0,1]" },
	back_edge     => sub { push @gb, "back_edge @_[0,1]" },
	down_edge     => sub { push @gb, "down_edge @_[0,1]" },
	cross_edge    => sub { push @gb, "cross_edge @_[0,1]" }
       );

$tb->dfs;

is($gb[ 0], "pre_edge a b",     "pre_edge");
is($gb[ 1], "pre_edge b c",     "pre_edge");
is($gb[ 2], "post_edge b c",    "post_edge");
is($gb[ 3], "non_tree_edge c a", "non_tree_edge");
is($gb[ 4], "back_edge c a",    "back_edge");
is($gb[ 5], "post_edge a b",    "post_edge");
is($gb[ 6], "pre_edge a d",     "pre_edge");
is($gb[ 7], "post_edge a d",    "post_edge");
is($gb[ 8], "non_tree_edge d b", "non_tree_edge");
is($gb[ 9], "cross_edge d b",   "cross_edge");
is($gb[10], "non_tree_edge a c", "non_tree_edge");
is($gb[11], "down_edge a c",    "down_edge");
is( @gb, 12 );

ok( $tb->tree->has_edge('a', 'b'), "tree edge");
ok( $tb->tree->has_edge('b', 'c'), "tree edge");
ok( $tb->tree->has_edge('a', 'd'), "tree edge");

ok(!$tb->tree->has_edge('c', 'a'), "non_tree edge");
ok(!$tb->tree->has_edge('d', 'b'), "non_tree edge");
ok(!$tb->tree->has_edge('a', 'c'), "non_tree edge");

is( $tb->tree, "a-b,a-d,b-c", "tree" );

is( $tb->preorder_by_vertex('a'), 0, "preorder of a" );
is( $tb->preorder_by_vertex('b'), 1, "preorder of b" );
is( $tb->preorder_by_vertex('c'), 2, "preorder of c" );
is( $tb->preorder_by_vertex('d'), 3, "preorder of d" );

is( $tb->vertex_by_preorder(0), 'a', "preorder of a" );
is( $tb->vertex_by_preorder(1), 'b', "preorder of b" );
is( $tb->vertex_by_preorder(2), 'c', "preorder of c" );
is( $tb->vertex_by_preorder(3), 'd', "preorder of d" );

is( $tb->postorder_by_vertex('a'), 3, "postorder of a" );
is( $tb->postorder_by_vertex('b'), 1, "postorder of b" );
is( $tb->postorder_by_vertex('c'), 0, "postorder of c" );
is( $tb->postorder_by_vertex('d'), 2, "postorder of d" );

is( $tb->vertex_by_postorder(3), 'a', "postorder of a" );
is( $tb->vertex_by_postorder(1), 'b', "postorder of b" );
is( $tb->vertex_by_postorder(0), 'c', "postorder of c" );
is( $tb->vertex_by_postorder(2), 'd', "postorder of d" );

my %pre = $tb->preorder_vertices();

is( $pre{'a'}, 0, "preorder of a" );
is( $pre{'b'}, 1, "preorder of b" );
is( $pre{'c'}, 2, "preorder of c" );
is( $pre{'d'}, 3, "preorder of d" );
is( keys %pre, 4 );

my %post = $tb->postorder_vertices();

is( $post{'a'}, 3, "postorder of a" );
is( $post{'b'}, 1, "postorder of b" );
is( $post{'c'}, 0, "postorder of c" );
is( $post{'d'}, 2, "postorder of d" );
is( keys %post, 4 );

my $gc = Graph->new(multiedged => 1);

$gc->add_path(qw(a b));
$gc->add_path(qw(a b));

my @gc;
my $tc =	
    Graph::Traversal::DFS->
    new($gc,
	next_alphabetic => 1,
	pre_edge      => sub { push @gc, "pre_edge @_[0,1]" },
	post_edge     => sub { push @gc, "post_edge @_[0,1]" },
	non_tree_edge => sub { push @gc, "non_tree_edge @_[0,1]" },
	back_edge     => sub { push @gc, "back_edge @_[0,1]" },
	down_edge     => sub { push @gc, "down_edge @_[0,1]" },
	cross_edge    => sub { push @gc, "cross_edge @_[0,1]" },
	seen_edge     => sub { push @gc, "seen_edge @_[0,1]" }
       );

$tc->dfs;

is( $gc[0], "pre_edge a b", "pre_edge" );
is( $gc[1], "post_edge a b", "post_edge" );
is( $gc[2], "seen_edge a b", "seen_edge" );
is( @gc, 3 );

my $gd = Graph->new;
$gd->add_edge(qw(0 1));
$gd->add_edge(qw(0 10));
$gd->add_edge(qw(0 9));
my @gd0;
my $td0 = Graph::Traversal::DFS->new($gd, next_numeric => 1, pre => sub { push @gd0, $_[0] });
$td0->dfs;
is( "@gd0", "0 1 9 10", "next_numeric" );
my @gd1;
my $td1 = Graph::Traversal::DFS->new($gd, next_alphabetic => 1, pre => sub { push @gd1, $_[0] });
$td1->dfs;
is( "@gd1", "0 1 10 9", "next_alphabetic" );

eval 'Graph::Traversal::DFS->new(next_alphabetic => 1)';
like($@, qr/Graph::Traversal: first argument is not a Graph/, "sane args");

eval 'Graph::Traversal::DFS->new($gd, next_alphazetic => 1)';
like($@, qr/Graph::Traversal: unknown attribute 'next_alphazetic'/, "zetic");

ok(!$td1->has_state('zot'), "has_state");

is($td1->get_state('zot'), undef, "get_state");

ok($td1->set_state('zot', 42), "set_state");

ok($td1->has_state('zot'), "has_state");

is($td1->get_state('zot'), 42, "get_state");

ok($td1->delete_state('zot'), "delete_state");

ok(!$td1->has_state('zot'), "has_state");

is($td1->get_state('zot'), undef, "get_state");

{
    # http://rt.cpan.org/NoAuth/Bug.html?id=4420
    use Graph::Directed;
    my $g = new Graph::Directed;
    ok($g = $g->add_edge('a','b'), "rt.cpan.org 4420");
    ok($g->has_edge('a','b'));
    ok($g = $g->add_edge('b','a'));
    ok($g->has_edge('b','a'));
    my @toposort;
    eval '@toposort = $g->toposort';
    like($@, qr/Graph::topological_sort: expected directed acyclic graph, got cyclic/);
    # http://rt.cpan.org/NoAuth/Bug.html?id=5168
    @toposort = $g->toposort(empty_if_cyclic => 1);
    is(@toposort, 0, "rt.cpan.org 5168");
    # http://rt.cpan.org/NoAuth/Bug.html?id=5167
    ok( $g->has_a_cycle, "rt.cpan.org 5167" );
    my $h = Graph->new;
    $h->add_edge(qw(a b));
    $h->add_edge(qw(a c));
    ok(!$h->has_a_cycle);
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::DFS->new($g0,
					   first_root => 'a',
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_successor => sub { shift; (reverse sort keys %{ $_[0] })[0] });
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->postorder;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "a e f b d c", "pre");
    is("@post", "f e d c b a", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a", "roots");
    ok( $t->is_root('a') );
    ok(!$t->is_root('b') );
    ok(!$t->is_root('c') );
}
