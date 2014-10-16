#!/usr/bin/perl
use strict;
BEGIN {
    if ( not $ENV{TEST_AUTHOR} ) {
        my $msg = 'Author test.  Set $ENV{TEST_AUTHOR} to a true value to run.';
        print "1..0 
    # $msg";
        exit 0;
    }
    require Test::More;
    Test::More->import();
}
use File::Find;
use IO::File;

if ( not $ENV{TEST_AUTHOR} ) {
    my $msg = 'Author test.  Set $ENV{TEST_AUTHOR} to a true value to run.';
    plan( skip_all => $msg );
}

my @skip = (
    qr(\.svn)
);

my $dir = 'blib/lib';
if (-d '../t') {
    $dir = '../lib';
}

my @filelist = ();
find( \&filelist, $dir);

sub filelist {
    my $name = $_;
    return if (-d $name);
    return if $File::Find::name =~m{\.svn}x;
    push @filelist, $File::Find::name;
}

plan( tests => scalar @filelist);

for my $file (sort @filelist) {
    check_file($file);
}

sub check_file {
    my $file = shift;
    my $fh = IO::File->new($file, O_RDONLY) or die "Cannot open $file";
    my $line_nr = 0;
    my $error_count = 0;

    while (my $line = $fh->getline() ) {
        # check for trailing whitespace
        # allow single whitespace on line to allow 
        # pod source blocks with empty lines
        #
        $line_nr++;
        if ($line =~m{ (:?[^\s]+|\s)\s\r?\n$ }x) {
            $error_count++;
            print "# trailing whitespace in $file line $line_nr at end of line\n"
        }

        # check for tabs and report their position
        my @tab_pos_from = ();
        my $pos = -1;
        while (1) {
            $pos = index($line, "\t", $pos + 1);
            last if $pos <0;
            push @tab_pos_from, $pos + 1;
        }
        if (@tab_pos_from) {
            print "# tab found in $file line $line_nr cols ${ \join(', ', @tab_pos_from) }\n";
            $error_count += scalar(@tab_pos_from);
        }
        
        if ($line=~m{\r}) {
            print "# CR (\\r) found in $file line $line_nr. Convert to LF only.\n";
            $error_count++;
        }
    }
    is $error_count, 0 , "$file characters";
}