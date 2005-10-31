#!/usr/bin/perl

use strict;
use File::Find; 
use Encode;
Encode::perlio_ok ("utf16") or die ("can't read utf16");

my $kfm = "KfM";
my $buildVersion;
my $marketingVersion;
my $copyright = "Copyright 2004 Massachusetts Institute of Technology";
my $shortCopyright = "Copyright 2004 MIT";
my $root;

my $usage = "Usage: KerberosVersion --build buildVersion --version versionString <root>\n";
while ($_ = shift @ARGV) {
    if    (/^--build/)   { $buildVersion = shift @ARGV; }
    elsif (/^--version/) { $marketingVersion = shift @ARGV; }
    else                 { $root = $_; }
}

$buildVersion or die $usage;
$marketingVersion or die $usage;

find (\&fixplists, $root); 

sub fixplists { 
    my $stringsFile = $File::Find::name;

    if (-f $stringsFile && ($_ =~ /^(Info|version)\.plist$/)) {
        print "Processing '$stringsFile'...\n";
        my $plist;
        open (my $input, "<$stringsFile") or die "$0: Can't open '$stringsFile': $!\n";
        {
            # Ignore end-of-line delimiters in the file
            local $/;
            undef $/; 
            $plist = <$input>;
        }
        close $input;
        
# replace version strings
        
        $plist =~ s@(<key>CFBundleVersion</key>\s*<string>)[^<]*(</string>)@${1}${buildVersion}${2}@xg;
# FIXME: CFBundleShortVersionString should be marketingVersion for next major release:
        $plist =~ s@(<key>CFBundleShortVersionString</key>\s*<string>)[^<]*(</string>)@${1}${buildVersion}${2}@xg;
        $plist =~ s@(<key>CFBundleGetInfoString</key>\s*<string>)[^<]*(</string>)@${1}${marketingVersion} ${copyright}${2}@xg;
        $plist =~ s@(<key>KfMDisplayVersion</key>\s*<string>)[^<]*(</string>)@${1}${marketingVersion}${2}@xg;
        $plist =~ s@(<key>KfMDisplayCopyright</key>\s*<string>)[^<]*(</string>)@${1}${shortCopyright}${2}@xg;
        $plist =~ s@(<key>NSHumanReadableCopyright</key>\s*<string>)[^<]*(</string>)@${1}${copyright}${2}@xg;
        
        open (my $output, ">$stringsFile") or die "$0: Can't open '$stringsFile' for writing: $!\n";
        print $output $plist;
        close $output;
    }
}
