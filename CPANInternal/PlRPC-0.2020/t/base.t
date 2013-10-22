
use strict;

print "1..3\n";
my $ok = require RPC::PlServer::Comm;
printf("%sok 1\n", ($ok ? "" : "not "));
$ok = require RPC::PlServer;
printf("%sok 2\n", ($ok ? "" : "not "));
$ok = require RPC::PlClient;
printf("%sok 3\n", ($ok ? "" : "not "));
