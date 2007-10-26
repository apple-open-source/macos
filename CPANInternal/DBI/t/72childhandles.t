#!perl -w

use strict;

#
# test script for the ChildHandles attribute
#

use DBI;

use Test::More;

my $HAS_WEAKEN = eval {
    require Scalar::Util;
    # this will croak() if this Scalar::Util doesn't have a working weaken().
    Scalar::Util::weaken(my $test = \"foo");
    1;
};
if (!$HAS_WEAKEN) {
    print "1..0 # Skipped: Scalar::Util::weaken not available\n";
    exit 0;
}

plan tests => 15;

{
    # make 10 connections
    my @dbh;
    for (1 .. 10) {
        my $dbh = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
        push @dbh, $dbh;
    }
    
    # get the driver handle
    my %drivers = DBI->installed_drivers();
    my $driver = $drivers{ExampleP};
    ok $driver;

    # get the kids, should be the same list of connections
    my $db_handles = $driver->{ChildHandles};
    is ref $db_handles, 'ARRAY';
    is scalar @$db_handles, scalar @dbh;

    # make sure all the handles are there
    my $found = 0;
    foreach my $h (@dbh) {
        ++$found if grep { $h == $_ } @$db_handles;
    }
    is $found, scalar @dbh;
}

# now all the out-of-scope DB handles should be gone
{
    my %drivers = DBI->installed_drivers();
    my $driver = $drivers{ExampleP};

    my $handles = $driver->{ChildHandles};
    my @db_handles = grep { defined } @$handles;
    is scalar @db_handles, 0, "All handles should be undef now";
}

my $dbh = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });


my $empty = $dbh->{ChildHandles};
is ref $empty, 'ARRAY', "ChildHandles should be an array-ref if wekref is available";
is scalar @$empty, 0, "ChildHandles should start with an empty array-ref";

# test child handles for statement handles
{
    my @sth;
    for (1 .. 200) {
        my $sth = $dbh->prepare('SELECT name FROM t');
        push(@sth, $sth);
    }
    my $handles = $dbh->{ChildHandles};
    is scalar @$handles, scalar @sth;

    # test a recursive walk like the one in the docs
    my @lines;
    sub show_child_handles {
        my ($h, $level) = @_;
        $level ||= 0;
        push(@lines, 
             sprintf "%sh %s %s\n", $h->{Type}, "\t" x $level, $h);
        show_child_handles($_, $level + 1) 
          for (grep { defined } @{$h->{ChildHandles}});
    }   
    show_child_handles($_) for (values %{{DBI->installed_drivers()}});
    print @lines[0..4];

    is scalar @lines, 202;
    like $lines[0], qr/^drh/;
    like $lines[1], qr/^dbh/;
    like $lines[2], qr/^sth/;
}

my $handles = $dbh->{ChildHandles};
my @live = grep { defined $_ } @$handles;
is scalar @live, 0, "handles should be gone now";

# test that the childhandle array does not grow uncontrollably
{
    for (1 .. 1000) {
        my $sth = $dbh->prepare('SELECT name FROM t');
    }
    my $handles = $dbh->{ChildHandles};
    cmp_ok scalar @$handles, '<', 1000;
    my @live = grep { defined } @$handles;
    is scalar @live, 0;
}
