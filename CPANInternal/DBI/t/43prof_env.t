#!perl -w
$|=1;

use strict;

#
# test script for using DBI_PROFILE env var to enable DBI::Profile
# and testing non-ref assignments to $h->{Profile}
#

BEGIN { $ENV{DBI_PROFILE} = 6 }	# prior to use DBI

use DBI;
use DBI::Profile;
use Config;
use Data::Dumper;

BEGIN {
    if ($DBI::PurePerl) {
	print "1..0 # Skipped: profiling not supported for DBI::PurePerl\n";
	exit 0;
    }
}

use Test::More tests => 11;

DBI->trace(0, "STDOUT");

my $dbh1 = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
is(ref $dbh1->{Profile}, "DBI::Profile");
is(ref $dbh1->{Profile}{Data}, 'HASH');
is(ref $dbh1->{Profile}{Path}, 'ARRAY');

my $dbh2 = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
is(ref $dbh2->{Profile}, "DBI::Profile");
is(ref $dbh2->{Profile}{Data}, 'HASH');
is(ref $dbh2->{Profile}{Path}, 'ARRAY');

is $dbh1->{Profile}, $dbh2->{Profile}, '$h->{Profile} should be shared';

$dbh1->do("set dummy=1");
$dbh1->do("set dummy=2");

my $profile = $dbh1->{Profile};

my $p_data = $profile->{Data};
is keys %$p_data, 3; # '', $sql1, $sql2
ok $p_data->{''};
ok $p_data->{"set dummy=1"};
ok $p_data->{"set dummy=2"};

__END__
