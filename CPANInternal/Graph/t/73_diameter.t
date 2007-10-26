use Test::More tests => 12;

use Graph;

my $g = Graph->new;

$g->add_edge(qw(e a));
$g->add_edge(qw(a r));
$g->add_edge(qw(r t));
$g->add_edge(qw(t h));
$g->add_edge(qw(h f));
$g->add_edge(qw(f r));
$g->add_edge(qw(r o));
$g->add_edge(qw(o m));
$g->add_edge(qw(m a));
$g->add_edge(qw(a b));
$g->add_edge(qw(b o));
$g->add_edge(qw(o v));
$g->add_edge(qw(v e));

is($g->graph_diameter, 7);
is($g->longest_path, 7);

my @p = $g->longest_path;
my $min = 0;
for my $i (0..$#p) {
    $min = $i if $p[$i] lt $p[$min];
}
if ($min) {
    push @p, splice @p, 0, $min;
}
print "# p = @p\n";
ok("@p" eq "a b t h f r o m" ||
   "@p" eq "a r t h f b o m");

is($g->average_path_length(),           293 / 90);

# a-b: a-b       : 1
# a-e: a-r-o-v-e : 4
# a-f: a-r-t-h-f : 4
# a-h: a-r-t-h   : 3
# a-m: a-r-o-m   : 3
# a-o: a-r-o     : 2
# a-r: a-r       : 1
# a-t: a-r-t     : 2
# a-v: a-r-o-v   : 3
#                  23 / 9 = 2.56
is($g->average_path_length('a'),        23 / 9);
is($g->average_path_length('b'),        33 / 9);
is($g->average_path_length('c'),        undef );
is($g->average_path_length('a', undef), 23 / 9);
is($g->average_path_length('b', undef), 33 / 9);
is($g->average_path_length(undef, 'a'), 27 / 9);
is($g->average_path_length(undef, 'b'), 33 / 9);

my $h = Graph->new;

$h->add_weighted_edge(qw(a b 2.3));
$h->add_weighted_edge(qw(a c 1.7));

is($h->shortest_path, 1.7);

