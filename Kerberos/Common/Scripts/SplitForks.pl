#!/usr/bin/perl

use strict;
use File::Find;

my $usage = "$0 Usage: SplitForks <sourceRoot> <destinationRoot>\n";
my $srcRoot = shift @ARGV or die $usage;
my $dstRoot = shift @ARGV or die $usage;

find (\&splitforks, $srcRoot); 

sub splitforks { 
    # don't do expensive checks on sources and directories:
    if (!-f $File::Find::name || /\.c$/ || /\.cp$/ || /\.cpp$/ || /\.h$/ || /\.jam$/) {
        return;
    }

    my $sourceFork = $File::Find::name . "/rsrc"; 
    if (-s $sourceFork) { 
        my $resources = `/Developer/Tools/DeRez -skip ckid "$File::Find::name"`;
        chomp $resources;  # we put the newline back later if there were resources

        if (length ($resources) > 0) {
            # The file exists and 
            my $type = `/Developer/Tools/GetFileInfo -t "$File::Find::name"`;
            chomp $type;
            
            my $creator = `/Developer/Tools/GetFileInfo -c "$File::Find::name"`;
            chomp $creator;

            my $attributes = `/Developer/Tools/GetFileInfo -a "$File::Find::name"`;
            chomp $attributes;
            
            # locations of the deconstructed resource file
            my $destinationRsrc = $dstRoot . substr ($File::Find::name, length ($srcRoot)) . "._r";
            my $destinationType = $dstRoot . substr ($File::Find::name, length ($srcRoot)) . "._type";
            my $destinationCreator = $dstRoot . substr ($File::Find::name, length ($srcRoot)) . "._creator";
            my $destinationAttributes = $dstRoot . substr ($File::Find::name, length ($srcRoot)) . "._attributes";

            # Write the resources into foo._rsrc
            open RESOURCES, "> $destinationRsrc" or die "$0: Unable to open $destinationRsrc for writing: $!\n";
            print RESOURCES $resources . "\n";
            close RESOURCES;
            
            # Write the file type into foo._type
            open TYPE, ">$destinationType" or die "$0: Unable to open $destinationType for writing: $!\n";
            print TYPE $type;
            close TYPE;

            # Write the file creator into foo._creator
            open CREATOR, ">$destinationCreator" or die "$0: Unable to open $destinationCreator for writing: $!\n";
            print CREATOR $creator;
            close CREATOR;

            # Write the file creator into foo._creator
            open ATTRIBUTES, ">$destinationAttributes" or die "$0: Unable to open $destinationAttributes for writing: $!\n";
            print ATTRIBUTES $attributes;
            close ATTRIBUTES;
        }
    }
}
