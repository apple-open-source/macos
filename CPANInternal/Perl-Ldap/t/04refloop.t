#!perl

use Net::LDAP qw(LDAP_UNAVAILABLE);

my $devnull = eval { require File::Spec; File::Spec->devnull } || "/dev/null";

unless (-e $devnull) {
  print "1..0 # Skip no null device\n";
  exit;
}

print "1..5\n";

$::destroy = 0;
{
  my $ldap = Net::LDAP::Dummy->new("host", async => 1);
  $ldap->bind; # create an internal ref loop
  require Data::Dumper, print Data::Dumper::Dumper($ldap->inner)
    if $ENV{TEST_VERBOSE};
}
print $::destroy ? "ok 1\n" : "not ok 1\n";

my $ref;
my $mesg;
$::destroy = 0;
{
  my $ldap = Net::LDAP::Dummy->new("host", async => 1);
  $mesg = $ldap->bind; # create an internal ref loop
  $ref = $ldap->inner->outer;
  print +($ref == $ldap) ? "not ok 2\n" : "ok 2\n";
}
print $::destroy ? "not ok 3\n" : "ok 3\n";
$ref = undef;
print +($mesg->code == LDAP_UNAVAILABLE) ? "ok 4\n" : "not ok 4\n";
undef $mesg;
print $::destroy ? "ok 5\n" : "not ok 5\n";


package Net::LDAP::Dummy;

use IO::File;

BEGIN { @ISA = qw(Net::LDAP); }

sub connect_ldap {
  my $ldap = shift;
  $ldap->{net_ldap_socket} = IO::File->new("+> $devnull");
}

sub DESTROY {
  my $self = shift;
  $::destroy = 1 unless tied(%$self);
  $self->SUPER::DESTROY;
}
