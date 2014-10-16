#!perl -w

use strict;
use Test::More;

BEGIN {
  my $tlib = $0;
  $tlib =~ s|/[^/]*$|/lib|;
  push(@INC, $tlib);
}
use t::Support;

if (t::Support::should_skip()) {
  plan skip_all => 'Tests unsupported on this OS/filesystem';
} else {
  plan tests => 213;
}

use File::Temp qw(tempfile);
use File::Path;
use File::ExtAttr qw(setfattr getfattr delfattr listfattr);
use IO::File;

my $TESTDIR = ($ENV{ATTR_TEST_DIR} || '.');
my ($fh, $filename) = tempfile( DIR => $TESTDIR );

close $fh or die "can't close $filename $!";

# Create a directory.
my $dirname = "$filename.dir";
eval { mkpath($dirname); };
if ($@) {
    warn "Couldn't create $dirname: $@";
}

my %vals;
for (my $i = 0; $i < 10; ++$i)
{
    $vals{"key$i"} = "val$i";
}

##########################
#  Filename-based tests  #
##########################

foreach ( $filename, $dirname ) {
    print "# using $_\n";

    foreach my $k (keys %vals)
    {
        # create it
        is (setfattr($_, $k, $vals{$k}, { create => 1 }), 1);

        # create it again -- should fail
        my $ret = setfattr($_, $k, $vals{$k}, { create => 1 });
        my $err = int $!;
        is ($ret, 0);
        is ($err, $!{EEXIST});

        # read it back
        is (getfattr($_, $k), $vals{$k});
    }
    
    # Check that the list contains all the attributes.
    my @attrs = listfattr($_);
    @attrs = sort(t::Support::filter_system_attrs(@attrs));
    my @ks = sort keys %vals;

    check_attrs(\@attrs, \@ks);

    # Clean up for next round of testing
    foreach my $k (keys %vals)
    {
        # delete it
        ok (delfattr($_, $k));

        # check that it's gone
        is (getfattr($_, $k), undef);
    }
}

##########################
# IO::Handle-based tests #
##########################

$fh = new IO::File("<$filename") or die "Unable to open $filename";

print "# using file descriptor ".$fh->fileno()."\n";

foreach (keys %vals)
{
    # create it
    is (setfattr($fh, $_, $vals{$_}, { create => 1 }), 1);

    # create it again -- should fail
    my $ret = setfattr($fh, $_, $vals{$_}, { create => 1 });
    my $err = int $!;
    is ($ret, 0);
    is ($err, $!{EEXIST});

    # read it back
    is (getfattr($fh, $_), $vals{$_});
}

# Check that the list contains all the attributes.
my @attrs = listfattr($fh);
@attrs = sort(t::Support::filter_system_attrs(@attrs));
my @ks = sort keys %vals;

check_attrs(\@attrs, \@ks);

# Clean up for next round of testing
foreach (keys %vals)
{
    # delete it
    ok (delfattr($filename, $_));

    # check that it's gone
    is (getfattr($filename, $_), undef);
}

END {
    unlink $filename if $filename;
    rmdir $dirname if $dirname;
};

sub check_attrs
{
    my @attrs = @{ $_[0] };
    my @ks = @{ $_[1] };

    is(scalar @attrs, scalar @ks);
    for (my $i = 0; $i < scalar @attrs; ++$i)
    {
        is($attrs[$i], $ks[$i]);
    }
}
