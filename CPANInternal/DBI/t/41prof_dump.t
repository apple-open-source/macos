#!perl -w

use strict;

#
# test script for DBI::ProfileDumper
# 

use DBI;

use Test::More;

BEGIN {
	if ($DBI::PurePerl) {
		plan skip_all => 'profiling not supported for DBI::PurePerl';
	}
	else {
		plan tests => 12;
	}
}

BEGIN {
    use_ok( 'DBI' );
    use_ok( 'DBI::ProfileDumper' );
}

my $dbh = DBI->connect("dbi:ExampleP:", '', '', 
                       { RaiseError=>1, Profile=>"DBI::ProfileDumper" });
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

open(PROF, "dbi.prof") or die $!;
my $prof = join('', <PROF>);
close PROF;

# has a header?
ok( $prof =~ /^DBI::ProfileDumper\s+([\d.]+)/, 'Found a version number' );
# Can't use like() because we need $1

# version matches VERSION? (DBI::ProfileDumper uses $self->VERSION so
# it's a stringified version object that looks like N.N.N)
is( $1, DBI::ProfileDumper->VERSION, 'Version numbers match' );

# check that expected key is there
like($prof, qr/\+\s+1\s+\Q$sql\E/m);

# unlink("dbi.prof"); # now done by 'make clean'

1;
