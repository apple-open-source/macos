#!perl -wl
# Using -l to ensure ProfileDumper is isolated from changes to $/ and $\ and such

$|=1;

use strict;

#
# test script for DBI::ProfileDumper
# 

use DBI;
use Config;
use Test::More;

BEGIN {
    plan skip_all => 'profiling not supported for DBI::PurePerl'
        if $DBI::PurePerl;

    # clock instability on xen systems is a reasonably common cause of failure
    # http://www.nntp.perl.org/group/perl.cpan.testers/2009/05/msg3828158.html
    # so we'll skip automated testing on those systems
    plan skip_all => "skipping profile tests on xen (due to clock instability)"
        if $Config{osvers} =~ /xen/ # eg 2.6.18-4-xen-amd64
        and $ENV{AUTOMATED_TESTING};

    plan tests => 15;
}

BEGIN {
    use_ok( 'DBI' );
    use_ok( 'DBI::ProfileDumper' );
}

my $dbh = DBI->connect("dbi:ExampleP:", '', '', 
                       { RaiseError=>1, Profile=>"2/DBI::ProfileDumper" });
isa_ok( $dbh, 'DBI::db' );
isa_ok( $dbh->{Profile}, "DBI::ProfileDumper" );
isa_ok( $dbh->{Profile}{Data}, 'HASH' );
isa_ok( $dbh->{Profile}{Path}, 'ARRAY' );

# do a little work
my $sql = "select mode,size,name from ?";
my $sth = $dbh->prepare($sql);
isa_ok( $sth, 'DBI::st' );
$sth->execute(".");

# check that flush_to_disk doesn't change Path if Path is undef (it
# did before 1.49)
{ 
    local $dbh->{Profile}->{Path} = undef;
    $sth->{Profile}->flush_to_disk();
    is($dbh->{Profile}->{Path}, undef);
}

$sth->{Profile}->flush_to_disk();
while ( my $hash = $sth->fetchrow_hashref ) {}

# force output
undef $sth;
$dbh->disconnect;
undef $dbh;

# wrote the profile to disk?
ok( -s "dbi.prof", 'Profile is on disk and nonzero size' );

# XXX We're breaking encapsulation here
open(PROF, "dbi.prof") or die $!;
my @prof = <PROF>;
close PROF;

print @prof;

# has a header?
like( $prof[0], '/^DBI::ProfileDumper\s+([\d.]+)/', 'Found a version number' );

# version matches VERSION? (DBI::ProfileDumper uses $self->VERSION so
# it's a stringified version object that looks like N.N.N)
$prof[0] =~ /^DBI::ProfileDumper\s+([\d.]+)/;
is( $1, DBI::ProfileDumper->VERSION, "Version numbers match in $prof[0]" );

like( $prof[1], qr{^Path\s+=\s+\[\s+\]}, 'Found the Path');
ok( $prof[2] =~ m{^Program\s+=\s+(\S+)}, 'Found the Program');

# check that expected key is there
like(join('', @prof), qr/\+\s+1\s+\Q$sql\E/m);

# unlink("dbi.prof"); # now done by 'make clean'

# should be able to load DBI::ProfileDumper::Apache outside apache
# this also naturally checks for syntax errors etc.
SKIP: {
    skip "developer-only test", 1
        unless -d ".svn" && -f "MANIFEST.SKIP";
    skip "Apache module not installed", 1
        unless eval { require Apache };
    require_ok('DBI::ProfileDumper::Apache')
}

1;
