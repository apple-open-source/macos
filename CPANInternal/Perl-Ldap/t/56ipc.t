#!perl

BEGIN {
  require "t/common.pl";
  start_server(ipc => 1);
}

print "1..12\n";

$ldap = client();
ok($ldap, "client");

$mesg = $ldap->bind($MANAGERDN, password => $PASSWD);

ok(!$mesg->code, "bind: " . $mesg->code . ": " . $mesg->error);

ok(ldif_populate($ldap, "data/50-in.ldif"), "data/50-in.ldif");

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

compare_ldif("50",$mesg,$mesg->sorted);

$ldap = client(ipc => 1);
ok($ldap, "ipc client");

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

compare_ldif("50",$mesg,$mesg->sorted);


