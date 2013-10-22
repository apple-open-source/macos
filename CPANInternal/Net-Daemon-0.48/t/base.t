# -*- perl -*-
#
#   $Id: base.t,v 1.2 1999/08/12 14:28:59 joe Exp $
#
BEGIN { $| = 1; print "1..1\n"; }
END {print "not ok 1\n" unless $loaded;}
use Net::Daemon;
$loaded = 1;
print "ok 1\n";


