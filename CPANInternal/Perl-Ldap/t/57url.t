#!perl

BEGIN {
  require "t/common.pl";
  start_server();
}

my $num_tests = @URL * 5 + 7;

print "1..$num_tests\n";

$ldap = client();
ok($ldap, "client");

$mesg = $ldap->bind($MANAGERDN, password => $PASSWD);

ok(!$mesg->code, "bind: " . $mesg->code . ": " . $mesg->error);

ok(ldif_populate($ldap, "data/50-in.ldif"), "data/50-in.ldif");

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

compare_ldif("50",$mesg,$mesg->sorted);

for my $url (@URL) {
  $ldap = client(url => $url);
  ok($ldap, "$url client");

  $mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
  ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

  compare_ldif("50",$mesg,$mesg->sorted);
}

