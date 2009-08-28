#!perl
#
# For this test to run you must defined the following in test.cfg
#   $EXTERNAL_TESTS = 1
#   %sortctrl with the following entries
#     host   => name of ldap server
#     base   => the base for the search
#     filter => the filter for the search
#     order  => the attribute name to order by
#
# The attribute given must have unique values over the entries
# returned from the search. This is because this test checks
# that the order of entries returned by 'attr' is the exact
# opposite of '-attr' this is not guaranteed if two entries have
# the same value for attr.
#
# Obviously the filter should be specific enough to ensure that
# a relatively small set of entries is returned
#
# TODO:
#
# This test should be expanded to test sort controls with
# more than one attribute specified.

use vars qw(%sortctrl);

BEGIN { require "t/common.pl" }

use Net::LDAP::LDIF;
use Net::LDAP::Control::Sort;
use Net::LDAP::Constant qw(
	LDAP_CONTROL_SORTREQUEST
	LDAP_CONTROL_SORTRESULT
	LDAP_SUCCESS
);

unless ($EXTERNAL_TESTS) {
  print "1..0 # Skip External tests disabled\n";
  exit 0;
}

my($host, $base, $filter, $order) = @sortctrl{qw(host base filter order)};

my $ldap = $host && Net::LDAP->new($host, version => 3);

unless ($ldap) {
  print "1..0 # Skip Cannot connect to host\n";
  exit 0;
}

my $dse  = $ldap && $ldap->root_dse;

unless ($dse and grep { $_ eq LDAP_CONTROL_SORTREQUEST } $dse->get_value('supportedControl')) {
  print "1..0 # Skip server does not support LDAP_CONTROL_SORTREQUEST\n";
  exit;
}

print "1..9\n";

Net::LDAP::LDIF->new(qw(- w))->write_entry($dse);

my $sort = Net::LDAP::Control::Sort->new(order => $order) or print "not ";
print "ok 1\n";

my $mesg = $ldap->search(
	      base	=> $base,
	      control	=> [$sort],
	      filter	=> $filter,
	    );

print "not " if $mesg->code;
print "ok 2\n";

my ($resp) = $mesg->control( LDAP_CONTROL_SORTRESULT ) or print "not ";
print "ok 3\n";

$resp && $resp->result == LDAP_SUCCESS or print "not ";
print "ok 4\n";

print "# ",$mesg->count,"\n";

my $dn1 = join ";", map { $_->dn } $mesg->entries;

$sort = Net::LDAP::Control::Sort->new(order => "-$order") or print "not ";
print "ok 5\n";

$mesg = $ldap->search(
	  base		=> $base,
	  control	=> [$sort],
	  filter	=> $filter,
	);

print "not " if $mesg->code;
print "ok 6\n";

($resp) = $mesg->control( LDAP_CONTROL_SORTRESULT ) or print "not ";
print "ok 7\n";

$resp && $resp->result == LDAP_SUCCESS or print "not ";
print "ok 8\n";

print "# ",$mesg->count,"\n";

my $dn2 = join ";", map { $_->dn } reverse $mesg->entries;

print "not " unless $dn1 eq $dn2;
print "ok 9\n";

