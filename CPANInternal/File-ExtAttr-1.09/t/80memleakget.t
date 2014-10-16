#!perl -w

use strict;
use Test::More;

# DEBUG: When debugging with valgrind or top, uncomment this stub for is().
# Otherwise the test results will be stored by Test::More, "distorting"
# the picture of memory usage -- it will include the memory usage
# of both File::ExtAttr and Test::More.
#
# sub is {}

BEGIN {
  my $tlib = $0;
  $tlib =~ s|/[^/]*$|/lib|;
  push(@INC, $tlib);
}
use t::Support;

if (t::Support::should_skip()) {
  plan skip_all => 'Tests unsupported on this OS/filesystem';
} else {
  plan tests => 6000;
}

use File::Temp qw(tempfile);
use File::Path;
use File::ExtAttr qw(setfattr getfattr);
use IO::File;

my $TESTDIR = ($ENV{ATTR_TEST_DIR} || '.');
my ($fh, $filename) = tempfile( DIR => $TESTDIR );

close $fh or die "can't close $filename $!";

# Create a directory.
my $dirname = "$filename.dir";
eval { mkpath($dirname); };
if ($@) {
    die "Couldn't create $dirname: $@";
}

#todo: try wierd characters in here?
#     try unicode?
my $key = "alskdfjadf2340zsdflksjdfa09eralsdkfjaldkjsldkfj";
my $val = "ZZZadlf03948alsdjfaslfjaoweir12l34kealfkjalskdfas90d8fajdlfkj./.,f";
my $key2 = $key.'2';

##########################
#  Filename-based tests  #
##########################

foreach ( $filename, $dirname ) {
    print "# using $_\n";

    setfattr($_, $key, $val) or die "setfattr failed on filename $_: $!";

    for (my $i = 0; $i < 1000; $i++) {
        # Check for the existing attribute.
        is(getfattr($_, $key), $val);

        # Check for the non-existing attribute.
        is(getfattr($_, $key2), undef);
    }

    # DEBUG: Uncomment when debugging.
    #sleep(5);
}

##########################
# IO::Handle-based tests #
##########################

$fh = new IO::File("<$filename") or die "Unable to open $filename";

print "# using file descriptor ".$fh->fileno()."\n";

setfattr($fh, $key, $val)
    or die "setfattr failed on file descriptor ".$fh->fileno().": $!"; 

for (my $i = 0; $i < 1000; $i++) {
    # Check for the existing attribute.
    is(getfattr($fh, $key), $val);

    # Check for the non-existing attribute.
    is(getfattr($fh, $key2), undef);
}

# DEBUG: Uncomment when debugging.
#sleep(5);

END {
    unlink $filename if $filename;
    rmdir $dirname if $dirname;
};
