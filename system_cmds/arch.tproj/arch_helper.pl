#!/usr/bin/perl -w
#
# Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
# arch_helper.pl is a perl script that automates the process of wrapping
# a command (in the DSTROOT) to use the architecture selection feature of
# the arch command.  The first argument is the full path (relative to root)
# of the command, and the second argument is the DSTROOT.  arch_helper.pl
# will move the command to a new directory in the DSTROOT, create a symbolic
# link from to old command path to the arch command, and create a plist file
# in /System/Library/archSettings to default to 32-bit over 64-bit
# architectures.

use strict;
use File::Basename ();
use File::Path ();
use File::Spec;
use IO::File;

my $ArchSettings = '/System/Library/archSettings';
my %Known = (
    '/usr/bin' => '/usr/archexec',
    '/usr/local/bin' => '/usr/local/archexec',
);
my $MyName = File::Basename::basename($0);

sub usage {
    print STDERR <<USAGE;
Usage: $MyName prog_path dstroot
       $MyName takes prog_path (full path relative to the dstroot)
       and dstroot, and moves the program to the corresponding archexec
       directory.  It then creates a symbolic from prog_path to the arch
       command.  Finally, a plist file is created in
       /System/Library/archSettings to default to using the 32-bit
       architectures.
USAGE
    exit 1;
}

usage() unless scalar(@ARGV) == 2;
my($vol, $dir, $file) = File::Spec->splitpath($ARGV[0]); # unix assumes $vol we be empty
$dir = File::Spec->canonpath($dir);
my $new = $Known{$dir};
die "$MyName: Unsupported directory $dir\n" unless defined($new);
my $dstroot = $ARGV[1];
die "$MyName: $dstroot: Not a full path\n" unless File::Spec->file_name_is_absolute($dstroot);
File::Path::mkpath(File::Spec->join($dstroot, $new), 1, 0755);
File::Path::mkpath(File::Spec->join($dstroot, $ArchSettings), 1, 0755);
my $execpath = File::Spec->canonpath(File::Spec->join($new, $file));
my $do = File::Spec->join($dstroot, $dir, $file);
my $dn = File::Spec->join($dstroot, $execpath);
rename($do, $dn) or die "$MyName: Can't move $file to $dn: $!\n";
print "renamed $do -> $dn\n";
my $l = File::Spec->abs2rel('/usr/bin/arch', $dir);
symlink($l, $do) or die "$MyName: Can't symlink $do -> $l: $!\n";
print "symlink $do -> $l\n";
my $plist = File::Spec->join($dstroot, $ArchSettings, $file . '.plist');
my $p = IO::File->new($plist, 'w') or die "$MyName: $plist: $!\n";
$p->print( <<PLIST );
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>ExecutablePath</key>
	<string>$execpath</string>
	<key>PreferredOrder</key>
	<array>
		<string>i386</string>
		<string>x86_64</string>
		<string>ppc</string>
		<string>ppc64</string>
	</array>
	<key>PropertyListVersion</key>
	<string>1.0</string>
</dict>
</plist>
PLIST
$p->close();
print "created $plist\n";
