#!perl

BEGIN {
  require "t/common.pl";
  start_server();
}

print "1..7\n";

$ldap = client();
ok($ldap, "client");

$mesg = $ldap->bind($MANAGERDN, password => $PASSWD);

ok(!$mesg->code, "bind: " . $mesg->code . ": " . $mesg->error);

ok(ldif_populate($ldap, "data/52-in.ldif"), "data/52-in.ldif");

# load modify LDIF
ok(ldif_populate($ldap, "data/52-mod.ldif", 'modify'), "data/52-mod.ldif");

# now search the database

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');

compare_ldif("52",$mesg,$mesg->sorted);

