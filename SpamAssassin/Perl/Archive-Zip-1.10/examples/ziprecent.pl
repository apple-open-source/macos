#!/usr/bin/perl -w
# Makes a zip file of the most recent files in a specified directory.
# By Rudi Farkas, rudif@bluemail.ch, 9 December 2000
# Usage: 
# ziprecent <dir> -d <ageDays> [-e <ext> ...]> [-h] [-msvc] [-q] [<zippath>]
# Zips files in source directory and its subdirectories
# whose file extension is in specified extensions (default: any extension).
#     -d <days>       max age (days) for files to be zipped (default: 1 day)
#     <dir>           source directory
#     -e <ext>        one or more space-separated extensions  
#     -h              print help text and exit
#     -msvc           may be given instead of -e and will zip all msvc source files  
#     -q              query only (list files but don't zip)
#     <zippath>.zip   path to zipfile to be created (or updated if it exists)
#
# $Revision: 1.1 $

use strict;

use Archive::Zip qw(:ERROR_CODES :CONSTANTS);
use Cwd; 
use File::Basename;
use File::Copy;
use File::Find;
use File::Path; 

# argument and variable defaults
#
my $maxFileAgeDays = 1;
my $defaultzipdir = 'h:/zip/_homework'; 
my ($sourcedir, $zipdir, $zippath, @extensions, $query);


# usage
#
my $scriptname = basename $0;
my $usage = <<ENDUSAGE;
$scriptname <dir> -d <ageDays> [-e <ext> ...]> [-h] [-msvc] [-q] [<zippath>]
Zips files in source directory and its subdirectories
whose file extension is in specified extensions (default: any extension).
    -d <days>       max age (days) for files to be zipped (default: 1 day)
    <dir>           source directory
    -e <ext>        one or more space-separated extensions  
    -h              print help text and exit
    -msvc           may be given instead of -e and will zip all msvc source files  
    -q              query only (list files but don't zip)
    <zippath>.zip   path to zipfile to be created (or updated if it exists)
ENDUSAGE


# parse arguments
#
while (@ARGV) {
    my $arg = shift;

    if ($arg eq '-d') {
        $maxFileAgeDays = shift;
        $maxFileAgeDays = 0.0 if $maxFileAgeDays < 0.0;
    }
    elsif ($arg eq '-e') {
        while ($ARGV[0] && $ARGV[0] !~ /^-/) {
            push @extensions, shift;    
        }
    }
    elsif ($arg eq '-msvc') {
        push @extensions, qw / bmp c cpp def dlg dsp dsw h ico idl mak odl rc rc2 rgs /;
    }
    elsif ($arg eq '-q') {
        $query = 1;
    }
    elsif ($arg eq '-h') {
        print STDERR $usage;
        exit;
    }
    elsif (-d $arg) {
        $sourcedir = $arg;
    }
    elsif ($arg eq '-z') {
        if ($ARGV[0]) {
            $zipdir = shift;    
        }
    }
    elsif ($arg =~ /\.zip$/) {
        $zippath = $arg;
    }
    else {
        errorExit("Unknown option or argument: $arg");
    }
}

# process arguments
#
errorExit("Please specify an existing source directory") unless defined($sourcedir) && -d $sourcedir;

my $extensions;
if (@extensions) {
    $extensions = join "|", @extensions;
}
else {
    $extensions = ".*";
}

# change '\' to '/' (avoids trouble in substitution on Win2k)
#
$sourcedir =~ s|\\|/|g;
$zippath =~ s|\\|/|g if defined($zippath);


# find files
#
my @files;
cwd $sourcedir;
find(\&listFiles, $sourcedir);
printf STDERR "Found %d file(s)\n", scalar @files;


# exit ?
#
exit if $query;
exit if @files <= 0;


# prepare zip directory
#
if (defined($zippath)) {
    # deduce directory from zip path
    $zipdir = dirname($zippath);
    $zipdir = '.' unless length $zipdir;
}
else {
    $zipdir= $defaultzipdir;
}

# make sure that zip directory exists
#
mkpath $zipdir unless -d $zipdir;
-d $zipdir or die "Can't find/make directory $zipdir\n";



# create the zip object
#
my $zip = Archive::Zip->new();


# read-in the existing zip file if any
#
if (defined $zippath && -f $zippath) {
    my $status = $zip->read($zippath);
    warn "Read $zippath failed\n" if $status != AZ_OK;
}

# add files
#
foreach my $memberName (@files)
{
    if (-d $memberName )
    {
        warn "Can't add tree $memberName\n"
            if $zip->addTree( $memberName, $memberName ) != AZ_OK;
    }
    else
    {
        $zip->addFile( $memberName )
            or warn "Can't add file $memberName\n";
    }
}


# prepare the new zip path 
#
my $newzipfile = genfilename();
my $newzippath = "$zipdir/$newzipfile";


# write the new zip file
#
my $status = $zip->writeToFileNamed($newzippath);
if ($status == AZ_OK) {
    # rename (and overwrite the old zip file if any)?
    #
    if (defined $zippath) {
        my $res = rename $newzippath, $zippath;
        if ($res) {
            print STDERR "Updated file $zippath\n";
        }
        else {
            print STDERR "Created file $newzippath, failed to rename to $zippath\n";
        }
    } 
    else {
        print STDERR "Created file $newzippath\n";
    }
}
else {
    print STDERR "Failed to create file $newzippath\n"; 
}



# subroutines
#

sub listFiles {
    if (/\.($extensions)$/) {
        cwd $File::Find::dir;
        return if -d $File::Find::name; # skip directories
        my $fileagedays = fileAgeDays($_);
        if ($fileagedays < $maxFileAgeDays) {
            printf STDERR "$File::Find::name    (%.3g)\n", $fileagedays;
            (my $filename = $File::Find::name) =~ s/^[a-zA-Z]://;  # remove the leading drive letter:
            push @files, $filename;
        }
    }
}

sub errorExit {
    printf STDERR "*** %s ***\n$usage\n", shift;
    exit;
}

sub mtime {
    (stat shift)[9];
}

sub fileAgeDays {
    (time() - mtime(shift)) / 86400;
}

sub genfilename {
    my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
    sprintf "%04d%02d%02d-%02d%02d%02d.zip", $year+1900, $mon+1, $mday, $hour, $min, $sec;
}

__END__

=head1 NAME

ziprecent.pl

=head1 SYNOPSIS

  ziprecent h:/myperl

  ziprecent h:/myperl -e pl pm -d 365

  ziprecent h:/myperl -q 

  ziprecent h:/myperl h:/temp/zip/file1.zip 
 

=head1 DESCRIPTION

=over 4

This script helps to collect recently modified files in a source directory 
into a zip file (new or existing).

It uses Archive::Zip.

=item C<  ziprecent h:/myperl  >

Lists and zips all files more recent than 1 day (24 hours)
in directory h:/myperl and it's subdirectories, 
and places the zip file into default zip directory.
The generated zip file name is based on local time (e.g. 20001208-231237.zip).


=item C<  ziprecent h:/myperl -e pl pm -d 365  >

Zips only .pl and .pm files more recent than one year.


=item C<  ziprecent h:/myperl -msvc  >

Zips source files found in a typical MSVC project.


=item C<  ziprecent h:/myperl -q  > 

Lists files that should be zipped.


=item C<  ziprecent h:/myperl h:/temp/zip/file1.zip  > 

Updates file named h:/temp/zip/file1.zip 
(overwrites an existing file if writable).


=item C<  ziprecent -h  > 

Prints the help text and exits.

 ziprecent.pl <dir> -d <days> [-e <ext> ...]> [-h] [-msvc] [-q] [<zippath>]
 Zips files in source directory and its subdirectories
 whose file extension is in specified extensions (default: any extension).
    -d <days>       max age (days) for files to be zipped (default: 1 day)
    <dir>           source directory
    -e <ext>        one or more space-separated extensions
    -h              print help text and exit
    -msvc           may be given instead of -e and will zip all msvc source files  
    -q              query only (list files but don't zip)
    <zippath>.zip   path to zipfile to be created (or updated if it exists)

=back


=head1 BUGS

Tested only on Win2k.

Does not handle filenames without extension.

Does not accept more than one source directory (workaround: invoke separately 
for each directory, specifying the same zip file).


=head1 AUTHOR

Rudi Farkas rudif@lecroy.com rudif@bluemail.ch

=head1 SEE ALSO

perl ;-)

=cut



