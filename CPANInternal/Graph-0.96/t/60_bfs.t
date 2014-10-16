use Test::More tests => 93;

use Graph::Directed;
use Graph::Undirected;
use Graph::Traversal::BFS;

my $g0 = Graph::Undirected->new;
my $g1 = Graph::Directed->new;
my $g2 = Graph::Undirected->new; # cyclic
my $g3 = Graph::Undirected->new; # unconnetced
my $g4 = Graph::Directed->new;   # cyclic
my $g5 = Graph::Directed->new;   # cyclic

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

sub simple {
    my $g = shift;
    my @v = $g->vertices;
    is(@_, @v, "vertices");
    my %v; $v{$_} ++ for @_;
    is(scalar(grep { ($v{$_} || 0) != 1 } @v), 0, "... once");
}

{
    my $t = Graph::Traversal::BFS->new($g0);

    is($t->unseen, $g0->vertices, "fresh traversal");
    is($t->seen,   0);
    is($t->seeing, 0);

    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->bfs;

    simple($g0, @t0);
    simple($g0, @t1);
    simple($g0, @t2);

    is($t->graph, $g0, "graph");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::BFS->new($g0,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->bfs;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "a b e c d f", "pre");
    is("@post", "a b e c d f", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a", "roots");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::BFS->new($g1,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1,
				       first_root     => 'b');
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->bfs;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "b c d a e f", "pre");
    is("@post", "b c d a e f", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "b a", "roots");
}

{
    my $t0 = Graph::Traversal::BFS->new($g0, next_alphabetic => 1);
    is($t0->next, "a",   "scalar next");
    $t0->terminate;
    is($t0->next, undef, "terminate");
    $t0->reset;
    is($t0->next, "a",   "after reset scalar next");
}

{
    my @pre;
    my @post;
    my $t = Graph::Traversal::BFS->new($g2,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->postorder;

    simple($g2, @t0);
    simple($g2, @t1);
    simple($g2, @t2);

    is("@pre",  "a b c",       "pre");
    is("@post", "a b c",       "post");
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
    my $t = Graph::Traversal::BFS->new($g3,
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_alphabetic => 1);
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->postorder;

    simple($g3, @t0);
    simple($g3, @t1);
    simple($g3, @t2);

    is("@pre",  "a b c d e f", "pre");
    is("@post", "a b c d e f", "post");
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
    my $t = Graph::Traversal::BFS->new($g0,
					   first_root => 'a',
				       pre  => sub { push @pre,  $_[0] },
				       post => sub { push @post, $_[0] },
				       next_successor => sub { shift; (reverse sort keys %{ $_[0] })[0] });
    my @t0 = $t->preorder;
    my @t1 = $t->postorder;
    my @t2 = $t->bfs;

    simple($g1, @t0);
    simple($g1, @t1);
    simple($g1, @t2);

    is("@pre",  "a e b f d c", "pre");
    is("@post", "a e b f d c", "post");
    is("@t0",   "@pre",        "t0");
    is("@t1",   "@post",       "t1");
    is("@t2",   "@post",       "t2");

    is($t->unseen, 0, "unseen none");
    is($t->seen,   6, "seen all");
    is($t->seeing, 0, "seeing none");
    is("@{[sort $t->seen]}", "a b c d e f", "seen all");
    is("@{[$t->roots]}", "a", "roots");
}
