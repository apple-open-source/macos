#!perl -w

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Linux-xattr.t'

##########################

# change 'tests => 2' to 'tests => last_test_to_print';

use strict;
use Test::More;
use Data::Dumper;

BEGIN {
  my $tlib = $0;
  $tlib =~ s|/[^/]*$|/lib|;
  push(@INC, $tlib);
}
use t::Support;

if (t::Support::should_skip()) {
  plan skip_all => 'Tests unsupported on this OS/filesystem';
} else {
  plan tests => 42;
}

use File::Temp qw(tempfile);
use File::Path;
use File::ExtAttr qw(setfattr getfattr delfattr listfattrns);
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
my $val = "ZZZadlf03948alsdjfaslfjaoweir12l34kealfkjalskdfas90d8fajdlfkj./.,f";

my $key2 = $key.'2';
my $key3 = $key.'3';

my @ns;

##########################
#  Filename-based tests  #
##########################

foreach ( $filename, $dirname ) {
    print "# using $_\n";

#for (1..30000) { #checking memory leaks

   #will die if xattr stuff doesn't work at all
   setfattr($_, "$key", $val, { namespace => 'user' })
     or die "setfattr failed on filename $_: $!"; 

   #set it
   is (setfattr($_, "$key", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($_, "$key", { namespace => 'user' }), $val);

   #set another
   is (setfattr($_, "$key2", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($_, "$key2", { namespace => 'user' }), $val);

   #set yet another
   is (setfattr($_, "$key3", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($_, "$key3", { namespace => 'user' }), $val);

   #check user namespace exists now
   @ns = listfattrns($_);
   is (grep(/^user$/, @ns), 1);

   #delete them
   ok (delfattr($_, "$key", { namespace => 'user' }));
   ok (delfattr($_, "$key2", { namespace => 'user' }));
   ok (delfattr($_, "$key3", { namespace => 'user' }));

   #check that they're gone
   is (getfattr($_, "$key", { namespace => 'user' }), undef);
   is (getfattr($_, "$key2", { namespace => 'user' }), undef);
   is (getfattr($_, "$key3", { namespace => 'user' }), undef);

   #check user namespace doesn't exist now
   SKIP: {
     skip "Unremoveable user attributes prevent testing namespace removal",
       1 if t::Support::has_system_attrs($_);

     @ns = listfattrns($_);
     is (grep(/^user$/, @ns), 0);
   }
#}
}

##########################
# IO::Handle-based tests #
##########################

$fh = new IO::File("<$filename") or die "Unable to open $filename";

print "# using file descriptor ".$fh->fileno()."\n";

#for (1..30000) { #checking memory leaks

   #will die if xattr stuff doesn't work at all
   setfattr($fh, "$key", $val, { namespace => 'user' })
     or die "setfattr failed on file descriptor ".$fh->fileno().": $!"; 

   #set it
   is (setfattr($fh, "$key", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($fh, "$key", { namespace => 'user' }), $val);

   #set another
   is (setfattr($fh, "$key2", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($fh, "$key2", { namespace => 'user' }), $val);

   #set yet another
   is (setfattr($fh, "$key3", $val, { namespace => 'user' }), 1);

   #read it back
   is (getfattr($fh, "$key3", { namespace => 'user' }), $val);

   #check user namespace exists now
   @ns = listfattrns($fh);
   is (grep(/^user$/, @ns), 1);

   #delete them
   ok (delfattr($fh, "$key", { namespace => 'user' }));
   ok (delfattr($fh, "$key2", { namespace => 'user' }));
   ok (delfattr($fh, "$key3", { namespace => 'user' }));

   #check that it's gone
   is (getfattr($fh, "$key", { namespace => 'user' }), undef);
   is (getfattr($fh, "$key2", { namespace => 'user' }), undef);
   is (getfattr($fh, "$key3", { namespace => 'user' }), undef);

   #check user namespace doesn't exist now
   SKIP: {
     skip "Unremoveable user attributes prevent testing namespace removal",
       1 if t::Support::has_system_attrs($fh);

     @ns = listfattrns($fh);
     is (grep(/^user$/, @ns), 0);
   }
#}
#print STDERR "done\n";
#<STDIN>;

END {
    unlink $filename if $filename;
    rmdir $dirname if $dirname;
};
