#!/usr/bin/perl

use strict;
use File::Find; 

my $version;
my $versionString;
my $copyright = "Copyright 2003 Massachusetts Institute of Technology";
my $shortCopyright = "Copyright 2003 MIT";
my $root;

my $usage = "Usage: KerberosVersion --version version --versionString string <root>\n";
while ($_ = shift @ARGV) {
    if    (/^--version$/)       { $version = shift @ARGV; }
    elsif (/^--versionString$/) { $versionString = shift @ARGV; }
    else                        { $root = $_; }
}

$version or die $usage;
$versionString or die $usage;

find (\&fixplists, $root); 

sub fixplists { 
    if (-f $File::Find::name && ($_ =~ /Info(|-macos|-macosclassic).plist/)) {
        print "Processing $File::Find::name...\n";
        my $plist;
        open PLIST, "$File::Find::name" or die "$0: Can't open $File::Find::name: $!\n";
        {
            local $/;
            undef $/; # Ignore end-of-line delimiters in the file
            $plist = <PLIST>;
        }
        close PLIST;
        
        # replace version strings
        $plist =~ s@(<key>CFBundleVersion</key>\s*<string>)[^<]*(</string>)@${1}${versionString}${2}@xg;
        $plist =~ s@(<key>CFBundleShortVersionString</key>\s*<string>)[^<]*(</string>)@${1}${version}${2}@xg;
        $plist =~ s@(<key>CFBundleGetInfoString</key>\s*<string>)[^<]*(</string>)@${1}${versionString} ${copyright}${2}@xg;
        $plist =~ s@(<key>KLSDisplayVersion</key>\s*<string>)[^<]*(</string>)@${1}${versionString} ${shortCopyright}${2}@xg;
        
        open PLIST, ">$File::Find::name" or die "$0: Can't open $File::Find::name for writing: $!\n";
        print PLIST $plist;
        close PLIST;
    }
}
