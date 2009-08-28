#!/usr/local/bin/perl

#isMember.pl 
#pass the common name of a group entry (assuming groupOfUniqueNames objectclass) and 
#a uid, the script will tell you if the uid is a member of the group or not.

$version = 3.0_01;

#in this version, the uid is a member of the given group if:
#are a member of the given group
#or are a member of a group who is a member of the given group
#or are a member of a dynamic group (currently only supported by Netscape Directory Server)


#Mark Wilcox mark@mjwilcox.com
#
#first version: August 8, 1999
#second version: August 15, 1999

#bugs: none ;)
#

#To Do: Change this into a module for Net::LDAP??
#       Add ability to handle various group objectclasses

use strict;
use Carp;
use Net::LDAP;
use URI;
use vars qw($opt_h $opt_p $opt_D $opt_w $opt_b $opt_n $opt_u );
use Getopt::Std;


my $DEBUG = 0; #set to 1 to turn debugging on

my $usage = "usage: $0 [-hpDwb] -n group_name -u uid ";

die $usage unless @ARGV;

getopts('h:p:D:w:b:n:u:');

die $usage unless ($opt_n && $opt_u);

#get configuration setup
$opt_h = "airwolf" unless $opt_h;
$opt_p = 389 unless $opt_p;
$opt_b = "o=airius.com" unless $opt_b;

my $isMember = 0; # by default you are not a member


my $ldap = new Net::LDAP ($opt_h, port=> $opt_p);

#will bind as specific user if specified else will be binded anonymously
$ldap->bind($opt_D, password=> $opt_p) || die "failed to bind as $opt_D"; 


#get user DN first
my @attrs = ["dn"];

my $mesg = $ldap->search(
               base => $opt_b,
	       filter => "uid=$opt_u",
	       attrs => @attrs
	      );

eval
{

    my $entry = $mesg->pop_entry();

   print "user is ",$entry->dn(),"\n" if $DEBUG;
   my $userDN = $entry->dn();

    #get original group DN
    $mesg = $ldap->search(
               base => $opt_b,
	       filter => "(&(cn=$opt_n)(objectclass=groupOfUniqueNames))",
	       attrs => @attrs
	       );

    $entry = $mesg->pop_entry();
   my $groupDN = $entry->dn();

   print "group is $groupDN\n" if $DEBUG;


   &getIsMember($groupDN,$userDN);

}; 


die $mesg->error if $mesg->code;


print "isMember is $isMember\n" if $DEBUG;
if ($isMember)
{
  print "$opt_u is a member of group $opt_n\n";
}
else
{
  print "$opt_u is not a member of group $opt_n\n";
}


$ldap->unbind();

sub getIsMember
{
   my ($groupDN,$userDN) = @_;

  # my $isMember = 0;

   print "in getIsMember:$groupDN\n" if $DEBUG;

   eval
   {

       #if user is a member then this will compare to true and we're done

      my $mesg = $ldap->compare($groupDN,attr=>"uniquemember",value=>$userDN);

      if ($mesg->code() == 6)
      {
        $isMember = 1;
        return $isMember;
      }
    };


   eval
   {
      #ok so you're not a member of this group, perhaps a member of the group
      #is also a group and you're a member of that group


      my @groupattrs = ["uniquemember","objectclass","memberurl"];

      $mesg = $ldap->search(
               base => $groupDN,
	       filter => "(|(objectclass=groupOfUniqueNames)(objectclass=groupOfUrls))",
	       attrs => @groupattrs
	       );

      my $entry = $mesg->pop_entry();



      #check to see if our entry matches the search filter

      my $urlvalues = $entry->get_value("memberurl", asref => 1);

      foreach my $urlval (@{$urlvalues})
      {

         my $uri = new URI ($urlval);


         my $filter = $uri->filter();

	 my @attrs = $uri->attributes();

         $mesg = $ldap->search(
               base => $userDN,
	       scope => "base",
	       filter => $filter,
	       attrs => \@attrs
	       );

        #if we find an entry it returns true
	#else keep searching
	
        eval
	{ 
          my $entry = $mesg->pop_entry();
	  print "ldapurl",$entry->dn,"\n" if $DEBUG;

	  $isMember  = 1;
	  return $isMember;
	};


      } #end foreach


      my $membervalues = $entry->get_value("uniquemember", asref => 1);
    
     foreach my $val (@{$membervalues})
     {
       my $return= &getIsMember($val,$userDN);

       #stop as soon as we have a winner
       last if $isMember;
     }
     

     die $mesg->error if $mesg->code;


     #if make it this far then you must be a member
  
   };

   return $0;
}
=head1 NAME

isMember.pl

=head1 DESCRIPTION

This script will determine if a given user is a member of a given group (including situations where the user is a member of another group, but that group is a member of the given group).

=head1 USAGE

perl isMember.pl -n "Acounting Managers" -u scarter
scarter is a member of group Acounting Managers

=head1 INFORMATION

Hi,

I've attached isMember.pl.

You pass it a group name (e.g. "Accounting Managers") and a user id
(e.g. "scarter"). It then tells you if scarter is a member of the group
Accounting Managers.

It assumes that the group name is stored as a value of the cn attribute
and that the group is a type of object class groupOfUniqueNames or
groupOfUrls.

The user name is assumed to be stored as a value of the uid attribute.

A membership requirement is met if:
a) the DN of the scarter (e.g. user) entry is stored as a value of
uniquemember attribute of the Accounting Managers (e.g. group) entry.
b) the group is a dynamic group (supported in Netscape Directory server)
and the member meets the search filter criteria


It will return if one of the following conditions are met:

a) scarter (e.g. user entry) is a member of the group Accounting
Managers (e.g. original group)
b) scarter (e.g. user entry) is a member of a group who is a member of
Accounting Managers (e.g. original group)
c) Accounting Managers (e.g. original group) is a Netscape dynamic group
;and scarter entry can be retrieved using the search filter of the
dynamic group URL.

I'm open to suggestions and/or critiques. I've hacked on this code long
enough, it probably needs a good cleaning, but I'll need some more
eyeballs on it since I've reached that point where the code is in my
head & not necessarily exactly as is on paper.

This script now requires the URI package to work (you need this package
to interact with Netscape Dynamic Groups). If you don't need the dynamic
group support, remove all of the dynamic group stuff and then you don't
need the URI package.

Note about Netscape Dynamic Groups:
Netscape Dynamic Groups are supported in Netscape Directory Server 4 and
later. They are objectclass of groupofurls entries, who's memberurls
attribute contains LDAP search URLs. If an entry matches the search
filter in the URL, then that entry is considered to be a member of the
group.

By managing groups this way instead of as values in a member attribute,
you can scale group memberships to the thousands if not millions.
Otherwise you're limited to about 14,000 members (which would be a very
big pain to manage). By using a search filter, all you have to do to
remove a member, is to make the offending entry, not match the search
filter anymore (e.g. change the attribute value in the entry or remove
the entry), as opposed as to having to go find each group and remove the
entry's dn from the member attribute and then re-add all of the still
valid member values back to the group entry.

As far as I know this is the first independent script to appear to
support Netscape Dynamic groups.

I'm next going to add dynamic group support to printMembers.pl

Hope y'all find this useful.

Mark

-----------------------------------------------------------------------------

Hi,
Here is an update to the isMember.pl script that I submitted last week.
As per the suggestion of Chris Ridd, the script returns true if the user
is a member of a group who is a member of the original group. I've
tested this down to 2 sub-group levels (e.g. user is a member of group C
which is a member of group B which is a member of the original group,
group A)

My next option to add is support of Netscape Dynamic Groups.

Here's a small list of the other things that I'm working on (and
hopefully will be able to submit to the list, some of them are for work
and may not be able to be released, but since I work for a university I
don't think there will be a problem):

1) script to add/remove members to a group
2) script to send mail to a list as long as the orignal email address is
from the owner of the group
3) a web LDAP management system. I've written a bare bones one in
Netscape's PerLDAP API, but I'd like to write something closer to
Netscape's Directory Server gateway that could possibly combine in
Text::Template for display & development. If someone would like to help
with this, let me know. I need it for work, so I'm going to do it (and
rather soon since it needs to be operational by end of September at the
latest).

Mark

-----------------------------------------------------------------------------

Howdy,

Here's my first draft on a new script that I'd like to submit for an example
script (or at least for general use) for Net::LDAP.

It's called isMember.pl. What it does is tell you if a given user is a member of
a particular group. You specify the group name and username on the command line
(and you can also specify the other common LDAP options such as host, binding
credentials etc via the command line as well).

Here is an example of how to use it and output:
perl isMember.pl -n "Acounting Managers" -u scarter
scarter is a member of group Acounting Managers

The script assumes that you make the DN of your groups with the cn attribute
(e.g. cn=Accounting Managers, ...) and that the group is of object class
groupOfUniqueNames. You can of course modify the script for your own use. While I
tested it with Netscape DS 4, it should work with any LDAP server (e.g. I'm not
relying on anything funky like dynamic groups).

And of course Your Mileage May Vary.

Mark

=cut
