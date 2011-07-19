eval "use Test::Pod::Coverage 0.08";
if ($@) {
    print "1..0 # Skip Test::Pod::Coverage not installed\n";
    exit;
} 

my $ARGS = {
    also_private    => [],
    trustme	    => [],
};

all_pod_coverage_ok( $ARGS );
