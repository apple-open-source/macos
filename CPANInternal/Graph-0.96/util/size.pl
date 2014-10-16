use lib 'lib';
use Graph;
use Devel::Size qw(size total_size);

my $N = 16384;

my $fmt = "%5s %8s %9s\n";
my $fmr = "%5d %8d %9.1f\n";

printf $fmt, "V", "S", "S/N";
my $g0 = Graph->new;
my $s0 = total_size($g0);
printf $fmr, 0, $s0, 0;

my $vr;
for (my $n = 1; $n <= $N; $n *= 2) {
    my $g0 = Graph->new;
    $g0->add_vertex($_) for 1..$n;
    my $s = total_size($g0);
    $vr = ($s - $s0) / $n;
    printf $fmr, $n, $s, $vr;
}

printf $fmt, "E", "S", "S/N";
my $g1 = Graph->new;
printf $fmr, 0, $s0, 0;

my $er;
for (my $n = 1; $n <= $N; $n *= 2) {
    my $g1 = Graph->new;
    $g1->add_edge(0, $_) for 1..$n;
    my $s = total_size($g1);
    $er = ($s - $s0 - $n * $vr) / $n;
    printf $fmr, $n, $s, $er;
}

printf "Vertices / MB = %8.1f\n", 1048576/$vr;
printf "Edges    / MB = %8.1f\n", 1048576/$er;
