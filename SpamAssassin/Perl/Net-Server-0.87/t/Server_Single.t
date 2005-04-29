BEGIN { $| = 1; print "1..1\n"; }

### load the module
END {print "not ok 1\n" unless $loaded;}
use Net::Server::Single;
$loaded = 1;
print "ok 1\n";

### not much to test
### this is only a personality for the MultiType
