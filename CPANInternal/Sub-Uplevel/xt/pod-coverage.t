use Test::More;

my $min_tpc = 1.08;
eval "use Test::Pod::Coverage $min_tpc";
plan skip_all => "Test::Pod::Coverage $min_tpc required for testing POD coverage"
    if $@;

my $min_pc = 0.17;
eval "use Pod::Coverage $min_pc";
plan skip_all => "Pod::Coverage $min_pc required for testing POD coverage"
    if $@;

my @modules = all_modules('lib');

plan tests => scalar @modules; 

for my $mod ( @modules ) {
    my $doc = "lib/$mod\.pod";
    $doc =~ s{::}{/}g;
    pod_coverage_ok( $mod, { pod_from => $doc } );
}

