#!/usr/bin/perl
#
# Copyright (c) 2006-2007 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
#
##########################################################################
#
# % build-libc.pl usr-local-lib-system out-directory
#
# This script takes the directory full of the contents libc-partial*.a and
# libsyscall*.a, and makes the necessary symbol aliases for those syscalls
# that aren't being wrapped in Libc.  The usr-local-lib-system is the
# /usr/local/lib/system or equivalent directory where the necessary symbol
# files from Libc and Libsyscall reside.
#
# A Makefile is created that will build libc*.a from the contents of the
# out-directory after symbol aliasing has been added.
#
# The out-directory path must be of the form ".../arch/form", where arch is
# the architecture being built and form is one of debug, dynamic and profile.
#
##########################################################################

use strict;
use DirHandle;
use File::Basename ();
use File::Copy ();
use File::Spec;
use IO::File;

my $MyName = File::Basename::basename($0);

my $OutDir;
my %Stub;
my %StubArgs;
my $StubFile = 'libsyscall.list';
my %Suffix = (
    debug => ['do', '_debug'],,
    dynamic => ['So', ''],,
    profile => ['po', '_profile'],,
);
my $SyscallBase = 'libc.syscall';

##########################################################################
# Scan the archive for existing wrappers, and remove them from the stub
# list.
##########################################################################
sub processLibc {
    my($arch, $dir, $sufname) = @_;
    local $_;
    my $file = File::Spec->join($dir, "libc-partial$sufname.a");
    my $f = IO::File->new("nm -g -arch $arch $file |");
    die "$MyName: nm -g -arch $arch $file: $!\n" unless defined($f);
    while(<$f>) {
	next unless s/^.* T //;
	chomp;
	delete($Stub{$_});
    }
}

##########################################################################
# Read the libc.syscall and any libc.syscall.arch file for additional aliases
# for the double underbar syscalls.
##########################################################################
sub readLibcSyscalls {
    my($arch, $dir) = @_;
    local $_;
    my @files = (File::Spec->join($dir, $SyscallBase));
    my $archfile = File::Spec->join($dir, "$SyscallBase.$arch");
    if(-r $archfile) {
	push(@files, $archfile);
    } elsif($arch =~ s/^armv.*/arm/) {
	$archfile = File::Spec->join($dir, "$SyscallBase.$arch");
	push(@files, $archfile) if -r $archfile;
    }
    foreach my $file (@files) {
	my $f = IO::File->new($file, 'r');
	die "$MyName: $file: $!\n" unless defined($f);
	while(<$f>) {
	    next if /^#/;
	    chomp;
	    my($k, $v) = split;
	    if(defined($v)) {
		$Stub{$k} = $v;
	    } else {
		delete($Stub{$k});
	    }
	}
    }
}

##########################################################################
# Read the libsyscall.list file for the system call names and number
# of arguments and store in %StubArgs.  Also, make an entry for a syscall
# stub.
##########################################################################
sub readStub {
    my $dir = shift;
    local $_;
    my $file = File::Spec->join($dir, $StubFile);
    my $f = IO::File->new($file, 'r');
    die "$MyName: $file: $!\n" unless defined($f);
    while(<$f>) {
	chomp;
	my($k, $v) = split;
	if(!($k =~ s/^#//)) {
	    $_ = $k;
	    s/^__//;
	    $Stub{$_} = $k;
	}
	$StubArgs{$k} = $v;
    }
}

sub usage {
    die "Usage: $MyName usr-local-lib-system out-directory\n";
}

usage() unless scalar(@ARGV) == 2;
my($usr_local_lib_system);
($usr_local_lib_system, $OutDir) = @ARGV;
die "$MyName: $usr_local_lib_system: No such directory\n" unless -d $usr_local_lib_system;
die "$MyName: $OutDir: No such directory\n" unless -d $OutDir;
my @pieces = File::Spec->splitdir($OutDir);
my $form = pop(@pieces);
my $arch = pop(@pieces);
my $suf = $Suffix{$form};
die "$MyName: $form: Unknown form\n" unless defined($suf);
my($suffix, $sufname) = @$suf;
readStub($usr_local_lib_system);
readLibcSyscalls($arch, $usr_local_lib_system);
processLibc($arch, $usr_local_lib_system, $sufname);

##########################################################################
# Invert the Stub hash, so the key will correspond to the file to process.
# The value will be an array ref containing all aliases.
##########################################################################
my %Inv;
while(my($k, $v) = each(%Stub)) {
    my $a = $Inv{$v};
    $a = $Inv{$v} = [] if !defined($a);
    push(@$a, $k);
}

##########################################################################
# Create the Makefile file
##########################################################################
my $path = File::Spec->join($OutDir, 'Makefile');
my $f = IO::File->new($path, 'w');
die "$MyName: $path: $!\n" unless defined($f);

##########################################################################
# List all the object files
##########################################################################
my $dir = DirHandle->new($OutDir);
die "$MyName: can't open $dir\n" unless defined($dir);
print $f "OBJS = libsystem.o \\\n";
my @objs;
while(defined($_ = $dir->read())) {
    next unless s/\.$suffix$/.o/;
    push(@objs, $_);
}
undef $dir;
printf $f "\t%s\n", join(" \\\n\t", @objs);

##########################################################################
# Add the build logic
##########################################################################
print $f <<XXX;

LIB = libc$sufname.a

all: \$(LIB)

\$(LIB): \$(OBJS)
	ar cq \$(.TARGET) `lorder \$(OBJS) | tsort -q`

.SUFFIXES: .$suffix

.$suffix.o:
	mv \$(.IMPSRC) \$(.TARGET)

XXX

##########################################################################
# Special case each binary that needs aliasing
##########################################################################
foreach my $k (sort(keys(%Inv))) {
    my $n = $k;
    $n =~ s/^_//;
    print $f "$n.o: $n.$suffix\n";
    print $f "\tld -arch $arch -r -keep_private_externs";
    foreach my $i (@{$Inv{$k}}) {
	$_ = $i;
	s/\$/\$\$/g;
	printf $f " -alias '$k' '$_'";
    }
    printf $f " -o \$(.TARGET) \$(.IMPSRC)\n";
}
