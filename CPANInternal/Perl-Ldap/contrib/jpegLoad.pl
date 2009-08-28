#!/usr/local/bin/perl
#  
#----------------------------------------------------------------------------
#
# This program was written by Clif Harden.
# It uses the PERL LDAP module.
# This LDAP module is available from the PERL CPAN
# system.
#
# Purpose: This program is designed to load jpeg file data into a LDAP
#          directory entry.
#
#
# $Id: jpegLoad.pl,v 1.2 2003/06/18 18:23:31 gbarr Exp $
#
# Revisions:
# $Log: jpegLoad.pl,v $
# Revision 1.2  2003/06/18 18:23:31  gbarr
# Remove all references to :all as it is not recomended
#
# Revision 1.1  2001/03/12 14:01:46  gbarr
# New contrib scripts from Clif Harden
#
#
#
#

use strict;
use Getopt::Std;
use Net::LDAP;
use Net::LDAP::Filter;
use Net::LDAP;
use Net::LDAP::Util qw( ldap_error_name ldap_error_text );
use Net::LDAP::Constant;


my $errstr = 0;
my $errmsg = "";

$errmsg = ldap_error_text($errstr);
   
#
# Initialize opt hash.
# You can change the defaults to match your setup.
# This can eliminate the need for many of the input
# options on the command line.
# 
my %opt = (
  'b' => 'dc=harden,dc=org',
  'h' => 'localhost',
  'd' => 0,
  'D' => 'cn=manager',
  'w' => 'password',
  'V' => '3',
  'a' => 'cn',
  'v' => 'commonName'
);

if ( @ARGV == 0 ) 
{
#
# print usage message.
#
Usage();
}

#
# Get command line options.
# 

getopts('b:f:h:d:D:w:V:a:v:',\%opt);


if ( !defined( $opt{'f'}) || !-e $opt{'f'} ) 
{
# 
# No jpeg file specified or the file does not exist.
#
print "$opt{'f'}\n";
Usage();
}

$/ = undef;
$\ = undef;
$, = undef;

#
# Slurp all of the jpeg file in at once.
#
open(IN, "<$opt{'f'}");
binmode(IN);
$_ = <IN>;   
close(IN); 

#
# build filter string
#
my $match = "( $opt{'a'}=$opt{'v'}  )";

#
# create filter object
#
my $f = Net::LDAP::Filter->new($match) or die "Bad filter '$match'";

#
# make ldap connection to directory.
#
my $ldap = new Net::LDAP($opt{'h'},
                         timeout => 10,
                         debug => $opt{'d'},
                        ) or die $@;

#
# Bind to directory.
#
$ldap->bind($opt{'D'}, password => "$opt{'w'}", version => $opt{'V'}) or die $@;

#
# Search directory for record that matches filter 
#
my $mesg = $ldap->search(
  base   => $opt{'b'},
  filter => $f,
  attrs  => [ "cn","jpegphoto" ],
) or die $@;

die $mesg->error,$mesg->code
	if $mesg->code;

#
# get record entry object
#
my $entry = $mesg->entry();

#
# get record DN
#
if ( !defined($entry) )
{
  print "No record for filter $match\n";
  $ldap->unbind;
  exit;
}

my $dn = $entry->dn();

print "\n";
print "dn:  $dn\n";
print "\n";

#
# initialize arrays
#
my @addMember = ();
my @memberChange = ();

push( @addMember, "jpegphoto" );     # attribute name
push( @addMember, $_ );   # attribute value

my $attr = $entry->get_value("jpegPhoto");
if(ref($attr))
{
  #
  # Entry already has a jpegPhoto, replace it.
  #
  push( @memberChange, "replace" );     # ldap replace operation
  push( @memberChange, \@addMember );     # ldap data to add 
}
else
{
  #
  # Entry does not have a jpegPhoto, add it.
  #
  push( @memberChange, "add" );     # ldap add operation
  push( @memberChange, \@addMember );     # ldap data to add 
}                                                                          

$mesg = $ldap->modify( $dn, changes => [ @memberChange ] ) or die $@;

if ( $mesg->code ) 
{
   $errstr = $mesg->code;
   print "Error code:  $errstr\n";
   $errmsg = ldap_error_text($errstr);
   print "$errmsg\n";
   
}

$ldap->unbind;

#----------------------------------------#
# Usage() - display simple usage message #
#----------------------------------------#
sub Usage
{
   print( "Usage: [-b] <base> | [-h] <host> | [-d] <number> | [-D] <DN> | [-w] <password> | [-a] <attribute> | [-v] <data> | [-f] <jpeg file> \n" );
   print( "\t-b    Search base.\n" );
   print( "\t-d    Debug mode.  Display debug messages to stdout.\n" );
   print( "\t-D    Authenication Distingushed Name.\n" );
   print( "\t-f    JPEG file to load in to attribute jpegPhoto.\n" );
   print( "\t      Required input option.\n" );
   print( "\t-h    LDAP directory host computer.\n" );
   print( "\t-w    Authenication password.\n" );
   print( "\t-a    Attribute that will be incorporated into the search filter.\n" );
   print( "\t-v    Data that will be incorporated into the search filter.\n" );
   print( "\t-V    LDAP version of the LDAP directory.\n" );
   print( "\n" );
   print( "\t      Perldoc pod documentation is included in this script.\n" );
   print( "\t      To read the pod documentation do the following;\n" );
   print( "\t      perldoc <script name>\n" );
   print( "\n" );
   print( "\n" );
   exit( 1 );
}                                                                               

__END__

=head1 NAME

jpegLoad.pl -  A script to load a jpeg picture into the jpegPhoto attribute of a directory entry.

=head1 SYNOPSIS

The intent of this script is to show the user how to load a 
picture that is in jpeg format into the jpegPhoto attribute of 
a directory entry.
The entry in question must have the schema defined to
allow the loading of the jpegPhoto attribute.

This script has been tested on a OpenLDAP 2.0.7 directory server
and a Netscape 4.x directory server.

You may need to change the first line of the PERL jpegLoad.pl script
to point to your file pathname of perl.

=head1 Input options.

 -b    Search base.
 -d    Debug mode.  Display debug messages to stdout.
 -D    Distingushed Name for authenication purposes.
 -f    JPEG file to load in to attribute jpegPhoto.
       Required input option and file must exist.
 -h    LDAP directory host computer.
 -w    Authenication password.
 -a    Attribute that will be incorporated into the search filter.
 -v    Data that will be incorporated into the search filter.
 -V    LDAP version of the LDAP directory.


 Usage: jpegLoad.pl -b <base> -h <host> -d <number> -D <DN> \
                    -w <password> -a <attribute> -v <data> \
                    -f <jpeg file>

Inside the script is a opt hash that can be initialized to 
default values that can eliminate the need for many of the 
input options on the command line.

-------------------------------------------------------------------

=head1 REQUIREMENTS

To use this program you will need the following.

At least PERL version 5.004.  You can get a stable version of PERL
from the following URL;
   http://cpan.org/src/index.html

Perl LDAP module.  You can get this from the following URL;
   ftp://ftp.duke.edu/pub/CPAN/modules/by-module/Net/

Bundled inside each PERL module is instructions on how to install the
module into your PERL system.

-------------------------------------------------------------------

=head1 INSTALLING THE SCRIPT

Install the jpegLoad.pl script anywhere you wish, I suggest
/usr/local/bin/jpegLoad.pl.

-------------------------------------------------------------------

Since the script is in PERL, feel free to modify it if it does not
meet your needs.  This is one of the main reasons I did it in PERL.
If you make an addition to the code that you feel other individuals
could use let me know about it.  I may incorporate your code
into my code.

=head1 AUTHOR

Clif Harden <charden@pobox.com>
If you find any errors in the code please let me know at
charden@pobox.com.

=head1 COPYRIGHT

Copyright (c) 2001 Clif Harden. All rights reserved. This program is
free software; you can redistribute it and/or modify it under the same
terms as Perl itself.

=cut
