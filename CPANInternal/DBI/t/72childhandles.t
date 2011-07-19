#!perl -w
$|=1;

use strict;

#
# test script for the ChildHandles attribute
#

use DBI;

use Test::More;

my $HAS_WEAKEN = eval {
    require Scalar::Util;
    # this will croak() if this Scalar::Util doesn't have a working weaken().
    Scalar::Util::weaken( my $test = [] ); # same test as in DBI.pm
    1;
};
if (!$HAS_WEAKEN) {
    chomp $@;
    print "1..0 # Skipped: Scalar::Util::weaken not available ($@)\n";
    exit 0;
}

plan tests => 16;

my $using_dbd_gofer = ($ENV{DBI_AUTOPROXY}||'') =~ /^dbi:Gofer.*transport=/i;

my $drh;

{
    # make 10 connections
    my @dbh;
    for (1 .. 10) {
        my $dbh = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });
        push @dbh, $dbh;
    }
    
    # get the driver handle
    $drh = $dbh[0]->{Driver};
    ok $drh;

    # get the kids, should be the same list of connections
    my $db_handles = $drh->{ChildHandles};
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
    my $handles = $drh->{ChildHandles};
    my @db_handles = grep { defined } @$handles;
    is scalar @db_handles, 0, "All handles should be undef now";
}

my $dbh = DBI->connect("dbi:ExampleP:", '', '', { RaiseError=>1 });

my $empty = $dbh->{ChildHandles};
is_deeply $empty, [], "ChildHandles should be an array-ref if wekref is available";

# test child handles for statement handles
{
    my @sth;
    my $sth_count = 20;
    for (1 .. $sth_count) {
        my $sth = $dbh->prepare('SELECT name FROM t');
        push @sth, $sth;
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
    my $drh = $dbh->{Driver};
    show_child_handles($drh, 0);
    print @lines[0..4];

    is scalar @lines, $sth_count + 2;
    like $lines[0], qr/^drh/;
    like $lines[1], qr/^dbh/;
    like $lines[2], qr/^sth/;
}

my $handles = $dbh->{ChildHandles};
my @live = grep { defined $_ } @$handles;
is scalar @live, 0, "handles should be gone now";

# test visit_child_handles
{
    my $info;
    my $visitor = sub {
        my ($h, $info) = @_;
        my $type = $h->{Type};
        ++$info->{ $type }{ ($type eq 'st') ? $h->{Statement} : $h->{Name} };
        return $info;
    };
    DBI->visit_handles($visitor, $info = {});
    is_deeply $info, {
        'dr' => {
            'ExampleP' => 1,
            ($using_dbd_gofer) ? (Gofer => 1) : ()
        },
        'db' => { '' => 1 },
    };

    my $sth1 = $dbh->prepare('SELECT name FROM t');
    my $sth2 = $dbh->prepare('SELECT name FROM t');
    DBI->visit_handles($visitor, $info = {});
    is_deeply $info, {
        'dr' => {
            'ExampleP' => 1,
            ($using_dbd_gofer) ? (Gofer => 1) : ()
        },
        'db' => { '' => 1 },
        'st' => { 'SELECT name FROM t' => 2 }
    };

}

# test that the childhandle array does not grow uncontrollably
SKIP: {
    skip "slow tests avoided when using DBD::Gofer", 2 if $using_dbd_gofer;

    for (1 .. 1000) {
        my $sth = $dbh->prepare('SELECT name FROM t');
    }
    my $handles = $dbh->{ChildHandles};
    cmp_ok scalar @$handles, '<', 1000;
    my @live = grep { defined } @$handles;
    is scalar @live, 0;
}

1;
