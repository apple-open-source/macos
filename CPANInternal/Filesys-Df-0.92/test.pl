# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..2\n"; }
END {print "not ok 1\n" unless $loaded;}
require 5.006;
use Config qw(%Config);
use Filesys::Df;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

my $dir = "/";

my $ref = Filesys::Df::df($dir);

defined($ref) and
	print"ok 2\n" or
	die "not ok 2\ndf\(\) call failed for \"$dir\" $!\n";

open(FILE, "./test.pl") or die "$! ./test.pl\n";
my $fh_ref = Filesys::Df::df(\*FILE);
close(FILE);

defined($fh_ref) and
	print"ok 3\n\n" or
	die "not ok 3\ndf\(\) call failed for \"test.pl\" $!\n";

print"Results for directory: \"$dir\" in 1K blocks:\n";
print "Total: $ref->{blocks}\n";
print "Free: $ref->{bfree}\n";
print "Available: $ref->{bavail}\n";
print "Used: $ref->{used}\n";
print "Percent Full: $ref->{per}\n";
if(exists($ref->{files})) {
	print "Total Inodes: $ref->{files}\n";
	print "Free Inodes: $ref->{ffree}\n";
	print "Inode Percent Full: $ref->{fper}\n";
}


print "\nResults for \"test.pl\" filehandle in 1K blocks:\n";
print "Total: $fh_ref->{blocks}\n";
print "Free: $fh_ref->{bfree}\n";
print "Available: $fh_ref->{bavail}\n";
print "Used: $fh_ref->{used}\n";
print "Percent Full: $fh_ref->{per}\n";
if(exists($fh_ref->{files})) {
	print "Total Inodes: $fh_ref->{files}\n";
	print "Free Inodes: $fh_ref->{ffree}\n";
	print "Inode Percent Full: $fh_ref->{fper}\n\n";
}


print"All tests successful!\n\n";

