#!/usr/bin/perl

use strict;
use File::Find; 

my $usage = "$0 Usage: MergeForks <root>\n";
my $root = shift @ARGV or die $usage;

find (\&mergeforks, $root); 

sub mergeforks { 
    my $resourceFile = $File::Find::name . "._r";
    if (-f $File::Find::name && -f $resourceFile) {
        # Copy in the resources:
        `/Developer/Tools/Rez "$resourceFile" -a -o "$File::Find::name"`;
        unlink ($resourceFile) or die "$0: Failed remove $resourceFile: $!\n";
        
        # Check for a _type file and if so, set the type:
        my $typeFile = $File::Find::name . "._type";
        if (-f $typeFile) {
            open TYPE, "$typeFile" or die "$0: Unable to open $typeFile: $!\n";
            my $type = <TYPE>;
            chomp $type;
            close TYPE;
            unlink ($typeFile) or die "$0: Failed remove $typeFile: $!\n";
            `/Developer/Tools/SetFile -t $type "$File::Find::name"`;
        }

        # Check for a _creator file and if so, set the type:
        my $creatorFile = $File::Find::name . "._creator";
        if (-f $creatorFile) {
            open CREATOR, "$creatorFile" or die "$0: Unable to open $creatorFile: $!\n";
            my $creator = <CREATOR>;
            chomp $creator;
            close CREATOR;
            unlink ($creatorFile) or die "$0: Failed remove $creatorFile: $!\n";
            `/Developer/Tools/SetFile -c $creator "$File::Find::name"`;
        }

        # Check for a _creator file and if so, set the type:
        my $attributesFile = $File::Find::name . "._attributes";
        if (-f $attributesFile) {
            open ATTRIBUTES, "$attributesFile" or die "$0: Unable to open $attributesFile: $!\n";
            my $attributes = <ATTRIBUTES>;
            chomp $attributes;
            close ATTRIBUTES;
            unlink ($attributesFile) or die "$0: Failed remove $attributesFile: $!\n";
            `/Developer/Tools/SetFile -a $attributes "$File::Find::name"`;
        }
    }
}
