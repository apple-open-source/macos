#!perl -w

use strict;

#
# test script for using DBI_PROFILE env var to enable DBI::Profile
#

BEGIN { $ENV{DBI_PROFILE} = 6 }	# prior to use DBI

use DBI;
use DBI::Profile;
use File::Spec;
use Config;
use Data::Dumper;

BEGIN {
    if ($DBI::PurePerl) {
	print "1..0 # Skipped: profiling not supported for DBI::PurePerl\n";
	exit 0;
    }
}

use Test::More tests => 13;

DBI->trace(0, "STDOUT");

my $dbh1 = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
is(ref $dbh1->{Profile}, "DBI::Profile");
is(ref $dbh1->{Profile}{Data}, 'HASH');
is(ref $dbh1->{Profile}{Path}, 'ARRAY');

my $dbh2 = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
is(ref $dbh2->{Profile}, "DBI::Profile");
is(ref $dbh2->{Profile}{Data}, 'HASH');
is(ref $dbh2->{Profile}{Path}, 'ARRAY');

# do a (hopefully) measurable amount of work on dbh1
$dbh1->do("set dummy=1");
my $sql1 = "select mode,size,name from ?";
my $sth1 = $dbh1->prepare($sql1);
for my $loop (1..50) { # enough work for low-res timers or v.fast cpus
    $sth1->execute(".");
    while ( my $hash = $sth1->fetchrow_hashref ) {}
}

# do a (hopefully) measurable amount of work on dbh2
$dbh1->do("set dummy=2");
my $sql2 = "select size,mode from ?";
my $sth2 = $dbh2->prepare($sql2);
for my $loop (1..50) { # enough work for low-res timers or v.fast cpus
    $sth2->execute(".");
    while ( my $hash = $sth2->fetchrow_hashref ) {}
}

is $dbh1->{Profile}, $dbh2->{Profile}, '$h->{Profile} should be shared';

my $profile = $dbh1->{Profile};

my $p_data = $profile->{Data};
is keys %$p_data, 5; # '', $sql1, $sql2
ok $p_data->{''};
ok $p_data->{$sql1};
ok $p_data->{$sql2};
ok $p_data->{"set dummy=1"};
ok $p_data->{"set dummy=2"};

__END__
