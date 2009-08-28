eval "use Test::Pod::Coverage";

if ($@) {
	print "1..0 # Skip Test::Pod::Coverage not installed", $/;
	exit;
} 

my $ARGS = {
	also_private    => [],
	trustme			=> [],
};

all_pod_coverage_ok($ARGS);
