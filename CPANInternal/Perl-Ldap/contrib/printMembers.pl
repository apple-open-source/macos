#!/usr/local/bin/perl

#printMembers.pl 
#given the name of a group (assume object class is groupOfUniqueNames) will
#display the members of the group including members of any groups that may be a member
#of the original group

#*Now Handles Netscape Dynamic Groups*
#
#By default it will display the DN of the member entries, you can specify a particular 
#attribute you wish to display instead (e.g. mail attribute)

#example: printMembers.pl -n "Accounting Managers" 


#optionally you can also specify the host, port, binded search and search base.

#Mark Wilcox mark@mjwilcox.com
#
#first version: August 8, 1999
#second version: August 15, 1999

use strict;
use Carp;
use Net::LDAP;
use URI;
use vars qw($opt_h $opt_p $opt_D $opt_w $opt_b $opt_n $opt_a );
use Getopt::Std;

my $usage = "usage: $0 [-hpDwba] -n group_name";

die $usage unless @ARGV;

getopts('h:p:D:w:b:n:a:');

die $usage unless ($opt_n);


my $DEBUG = 0; #DEBUG 1 if you want debugging info
#get configuration setup
$opt_h = "airwolf" unless $opt_h;
$opt_p = 389 unless $opt_p;
$opt_b = "o=airius.com" unless $opt_b;


my $isGroup = 0; #checks for group or not

my $ldap = new Net::LDAP ($opt_h, port=> $opt_p);

#will bind as specific user if specified else will be binded anonymously
$ldap->bind($opt_D, password=> $opt_p) || die "failed to bind as $opt_D"; 


#get the group DN
my @attrs = ['dn'];
eval
{
   my $mesg = $ldap->search(
               base => $opt_b,
	       filter => "(&(cn=$opt_n)(objectclass=groupOfUniqueNames))",
	       attrs => @attrs
	       );

   die $mesg->error if $mesg->code;

   my $entry = $mesg->pop_entry();

   my $groupDN = $entry->dn();

   &printMembers($groupDN,$opt_a);
   $isGroup = 1;
};

print "$opt_n is not a group" unless ($isGroup);

$ldap->unbind();


sub printMembers
{
  my ($dn,$attr) = @_;

  my @attrs = ["uniquemember","memberurl"];

  my $mesg = $ldap->search(
               base => $dn,
	       scope => 'base',
	       filter => "objectclass=*",
	       attrs => @attrs
	      );

  die $mesg->error if $mesg->code;

  #eval protects us if nothing is returned in the search

  eval
  {

     #should only be 1 entry
     my $entry = $mesg->pop_entry();

     print "\nMembers of group: $dn\n";

     #returns an array reference
     my $values = $entry->get_value("uniquemember", asref => 1);

     foreach my $val (@{$values})
     {
       my $isGroup = 0; #lets us know if the entry is also a group, default no

       #change val variable to attribute

       #now get entry of each member
       #is a bit more efficient since we use the DN of the member
       #as our search base, greatly reducing the number of entries we 
       #must search through for a match to 1 :)

       my @entryAttrs = ["objectclass","memberurl",$attr];

       $mesg = $ldap->search(
               base => $val,
	       scope => 'base',
	       filter => "objectclass=*",
	       attrs => @entryAttrs
	      );

      die $mesg->error if $mesg->code;

      eval
     {
        my $entry = $mesg->pop_entry();


        if ($attr)
	{
          my  $values = $entry->get_value($attr, asref => 1);

           foreach my $vals (@{$values})
           {
             print $vals,"\n";
           }
	}
        else
	{
           print "$val\n";
	}

        my $values = $entry->get_value("objectclass", asref => 1);

        # This value is also a group, print the members of it as well  


        &printMembers($entry->dn(),$attr) if (grep /groupOfUniqueNames/i, @{$values});
     };
   } 
        my $urls = $entry->get_value("memberurl", asref => 1);
	&printDynamicMembers($entry->dn(),$urls,$attr) if ($urls); 
 };
    return 0;
  }



#prints out a search results
#for members of dynamic group (as supported by the Netscape Directory Server)

#*Note this may or may not return all of the resulting members and their attribute values 
#depending on how the LDAP connection is binded. Normally users who are not binded as the Directory Manager
#are restricted to 2000 or less total search results. 

#In theory a dynamic group could have a million or more entries
sub printDynamicMembers
{
   my ($entryDN,$urls,$attr) = @_;

   print "\nMembers of dynamic group: $entryDN\n";


   foreach my $url (@{$urls})
   {
     print "url is $url\n" if $DEBUG;
     my $uri;
     eval
     {
      $uri =  URI->new($url);
     } ;

     print "ref ",ref($uri),"\n" if $DEBUG;

     my $base = $uri->dn();

     print "base is $base\n" if $DEBUG;
     my $scope = $uri->scope(); 

     my $filter = $uri->filter();

     my @attrs = [$attr];

     my $mesg = $ldap->search(
               base => $base,
	       scope => $scope,
	       filter => $filter,
	       attrs => @attrs
	       );
 
     #print results

     my $entry;
     while ($entry = $mesg->pop_entry())
     { 

        if ($attr)
	{
          my  $values = $entry->get_value($attr, asref => 1);

           foreach my $vals (@{$values})
           {
             print $vals,"\n";
           }
	}
        else
	{
           print $entry->dn(),"\n";
	} 
     }

    }
  return 0;
} 



=head1 NAME

printMembers.pl

=head1 DESCRIPTION

Prints out the members of a given group, including members of groups that are also members of the given group.

Defaults to printing out members by DN, but you can specify other attributes for display

=head1 USAGE

perl printMembers.pl -n "Accounting Managers"

  Members of group: cn=Accounting Managers,ou=groups,o=airius.com
  uid=scarter, ou=People, o=airius.com
  uid=tmorris, ou=People, o=airius.com
  cn=HR Managers,ou=groups,o=airius.com

  Members of group: cn=HR Managers,ou=groups,o=airius.com
  uid=kvaughan, ou=People, o=airius.com
  uid=cschmith, ou=People, o=airius.com
  cn=PD Managers,ou=groups,o=airius.com

  Members of group: cn=PD Managers,ou=groups,o=airius.com
  uid=kwinters, ou=People, o=airius.com
  uid=trigden, ou=People, o=airius.com

Here's an example of the same group but instead print the cn attribute
of each entry:

  Members of group: cn=Accounting Managers,ou=groups,o=airius.com
  Sam Carter
  Ted Morris
  HR Managers

  Members of group: cn=HR Managers,ou=groups,o=airius.com
  Kirsten Vaughan
  Chris Schmith
  PD Managers

  Members of group: cn=PD Managers,ou=groups,o=airius.com
  Kelly Winters
  Torrey Rigden

  And same group but with the mail attribute:

  Members of group: cn=Accounting Managers,ou=groups,o=airius.com
  scarter@airius.com
  tmorris@airius.com

  Members of group: cn=HR Managers,ou=groups,o=airius.com
  kvaughan@airius.com
  cschmith@airius.com

  Members of group: cn=PD Managers,ou=groups,o=airius.com
  kwinters@airius.com
  trigden@airius.com

=cut
