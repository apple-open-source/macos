#!perl -w

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Linux-xattr.t'

#########################

# change 'tests => 2' to 'tests => last_test_to_print';

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
  plan tests => 8;
}

use File::Temp qw(tempfile);
use File::Path;
use File::ExtAttr qw(setfattr getfattr delfattr);

my $TESTDIR = ($ENV{ATTR_TEST_DIR} || '.');
my ($fh, $filename) = tempfile( DIR => $TESTDIR );
close $fh or die "can't close $filename $!";

# Create a directory.
my $dirname = "$filename.dir";
eval { mkpath($dirname); };
if ($@) {
    warn "Couldn't create $dirname: $@";
}

#todo: try wierd characters in here?
#     try unicode?
my $key = "alskdfjadf2340zsdflksjdfa09eralsdkfjaldkjsldkfj";
my $val = "ZZZadlf03948alsdjfaslfjaoweir12l34kealfkjalskdfas90d8fajdlfkj./.,f";

foreach ( $filename, $dirname ) {
    print "# using $_\n";

#for (1..30000) { #checking memory leaks

    #will die if xattr stuff doesn't work at all
    setfattr($_, "$key", $val) or die "setfattr failed on $_: $!"; 

    #set it
    is (setfattr($_, "$key", $val), 1);

    #read it back
    is (getfattr($_, "$key"), $val);

    #delete it
    ok (delfattr($_, "$key"));

    #check that it's gone
    is (getfattr($_, "$key"), undef);
#}
}

END {
    unlink $filename if $filename;
    rmdir $dirname if $dirname;
};
