eval "use Test::Pod";
if ($@) {
	    print "1..0 # Skip Test::Pod not installed\n";
		    exit;
} 
 
 my @PODS = qw#../blib#;

all_pod_files_ok( all_pod_files(@PODS) );
