#!/usr/bin/perl

use strict;

# exec make, but first run make with a target of no64.  If it outputs YES,
# then remove 64-bit arches from RC_CFLAGS and RC_ARCHS, remover RC_xxx,
# where xxx is a 64-bit architecture.  If there are no archs left, just
# return success.

my $dir = '.';

for(0...$#ARGV) {
    if($ARGV[$_] eq '-C') {
	$dir = $ARGV[$_ + 1];
	last;
    }
}

my $no64 = `make -C $dir no64`;
chomp($no64);

if($no64 eq 'YES') {
    my @archs;
    my @arch64;
    my @cflags;
    my $arch = 0;
    for(split(" ", $ENV{RC_CFLAGS})) {
	if($arch) {
	    if(/64/) {
		push(@arch64, $_);
	    } else {
		push(@cflags, '-arch', $_);
		push(@archs, $_);
	    }
	    $arch = 0;
	    next;
	}
	if($_ eq '-arch') {
	    $arch = 1;
	    next;
	}
	push(@cflags, $_);
    }
    unless(scalar(@archs) > 0) {
	print "Not building:\tmake @ARGV\n";
	exit 0;
    }
    $ENV{RC_CFLAGS} = join(' ', @cflags);
    $ENV{RC_ARCHS} = join(' ', @archs);
    push(@ARGV, "RC_CFLAGS=$ENV{RC_CFLAGS}", "RC_ARCHS=$ENV{RC_ARCHS}");
    for(@arch64) {
	delete($ENV{"RC_$_"});
	push(@ARGV, "RC_$_=");
    }
}
print "make @ARGV\n";
exec {'make'} 'make', @ARGV;
