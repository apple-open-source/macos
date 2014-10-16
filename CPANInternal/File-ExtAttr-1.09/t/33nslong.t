#!perl -w

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Linux-xattr.t'

#########################

# change 'tests => 2' to 'tests => last_test_to_print';

# XXX: Refactor the common bits between this and 11basic.t
# into Test::Class classes?

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
  plan tests => 24;
}

use File::Temp qw(tempfile);
use File::Path;
use File::ExtAttr qw(setfattr getfattr delfattr);
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

#todo: try wierd characters in here?
#     try unicode?
my $key = "alskdfjadf2340zsdflksjdfa09eralsdkfjaldkjsldkfj";
my $longval = 'A' x $File::ExtAttr::MAX_INITIAL_VALUELEN;
my $longval2 = 'A' x ($File::ExtAttr::MAX_INITIAL_VALUELEN + 11);

##########################
#  Filename-based tests  #
##########################

foreach ( $filename, $dirname ) {
    print "# using $_\n";

#for (1..30000) { #checking memory leaks
   #check a really big one, bigger than $File::ExtAttr::MAX_INITIAL_VALUELEN
   #Hmmm, 3991 is the biggest number that doesn't generate "no space left on device"
   #on my /var partition, and 920 is the biggest for my loopback partition.
   #What's up with that?
   #setfattr($_, "$key-2", ('x' x 3991)) or die "setfattr failed on $_: $!"; 
    setfattr($_, "$key", $longval, { namespace => 'user' })
        or die "setfattr failed on $_: $!"; 

    #set it
    is (setfattr($_, "$key", $longval, { namespace => 'user' }), 1);

    #read it back
    is (getfattr($_, "$key", { namespace => 'user' }), $longval);

    #delete it
    ok (delfattr($_, "$key", { namespace => 'user' }));

    #check that it's gone
    is (getfattr($_, "$key", { namespace => 'user' }), undef);

    #set it
    is (setfattr($_, "$key", $longval2, { namespace => 'user' }), 1);

    #read it back
    is (getfattr($_, "$key", { namespace => 'user' }), $longval2);

    #delete it
    ok (delfattr($_, "$key", { namespace => 'user' }));

    #check that it's gone
    is (getfattr($_, "$key", { namespace => 'user' }), undef);
#}
}

##########################
# IO::Handle-based tests #
##########################

$fh = new IO::File("<$filename") or die "Unable to open $filename";

print "# using file descriptor ".$fh->fileno()."\n";

#for (1..30000) { #checking memory leaks
   #check a really big one, bigger than $File::ExtAttr::MAX_INITIAL_VALUELEN
   #Hmmm, 3991 is the biggest number that doesn't generate "no space left on device"
   #on my /var partition, and 920 is the biggest for my loopback partition.
   #What's up with that?
   #setfattr($filename, "$key-2", ('x' x 3991)) or die "setfattr failed on $filename: $!"; 
   setfattr($fh, "$key", $longval, { namespace => 'user' })
    or die "setfattr failed on file descriptor ".$fh->fileno().": $!"; 

   #set it
   is (setfattr($fh, "$key", $longval, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($fh, "$key", { namespace => 'user' }), $longval);

   #delete it
   ok (delfattr($fh, "$key", { namespace => 'user' }));

   #check that it's gone
   is (getfattr($fh, "$key", { namespace => 'user' }), undef);

   #set it
   is (setfattr($fh, "$key", $longval2, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($fh, "$key", { namespace => 'user' }), $longval2);

   #delete it
   ok (delfattr($fh, "$key", { namespace => 'user' }));

   #check that it's gone
   is (getfattr($fh, "$key", { namespace => 'user' }), undef);
#}
#print STDERR "done\n";
#<STDIN>;

END {
    unlink $filename if $filename;
    rmdir $dirname if $dirname;
};
