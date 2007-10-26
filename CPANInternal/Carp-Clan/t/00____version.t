#!perl -w

use strict;
no strict "vars";

# ======================================================================
#   $version = $Carp::Clan::VERSION;
# ======================================================================

$Carp::Clan::VERSION = $Carp::Clan::VERSION = 0;

print "1..3\n";

$n = 1;

eval { require Carp::Clan; };
unless ($@)
{print "ok $n\n";} else {print "not ok $n\n";}
$n++;

eval { Carp::Clan->import( qw(^Carp\\b) ); };
unless ($@)
{print "ok $n\n";} else {print "not ok $n\n";}
$n++;

if ($Carp::Clan::VERSION eq '5.3')
{print "ok $n\n";} else {print "not ok $n\n";}
$n++;

__END__

