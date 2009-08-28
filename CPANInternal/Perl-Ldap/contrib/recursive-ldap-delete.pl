#!/usr/bin/perl -w
#
# recursive-ldap-delete.pl
#
# originally by Mike Jackson <mj@sci.fi>
# shortened by Peter Marschall <peter@adpm.de>
# based on ideas by Norbert Kiesel <nkiesel@tbdetworks.com>
#
# ToDo: check errors, handle references, ....

use strict;
use Net::LDAP;

my $server      = "localhost";
my $binddn      = "cn=directory manager";
my $bindpasswd  = "foobar";
my $delbranch   = "ou=users,dc=bigcorp,dc=com";		# branch to remove

my $ldap        = Net::LDAP->new( $server ) or die "$@";
$ldap->bind( $binddn, password => $bindpasswd, version => 3 );

my $search      = $ldap->search( base   => $delbranch,
                                 filter => "(objectclass=*)" );

# delete the entries found in a sorted way:
# those with more "," (= more elements) in their DN, which are deeper in the DIT, first
# trick for the sorting: tr/,// returns number of , (see perlfaq4 for details)
foreach my $e (sort { $b->dn =~ tr/,// <=> $a->dn =~ tr/,// } $search->entries()) {
  $ldap->delete($e);
}  

$ldap->unbind();
