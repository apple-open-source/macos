#!perl

BEGIN {
  require "t/common.pl";
  start_server();
}

print "1..15\n";

$ldap = client();
ok($ldap, "client");

$mesg = $ldap->bind($MANAGERDN, password => $PASSWD);

ok(!$mesg->code, "bind: " . $mesg->code . ": " . $mesg->error);

ok(ldif_populate($ldap, "data/51-in.ldif"), "data/51-in.ldif");


# now search the database

# Exact searching
$mesg = $ldap->search(base => $BASEDN, filter => 'sn=jensen');
compare_ldif("51a",$mesg,$mesg->sorted);

# Or searching
$mesg = $ldap->search(base => $BASEDN, filter => '(|(objectclass=groupofnames)(sn=jones))');
compare_ldif("51b",$mesg,$mesg->sorted);

# And searching
$mesg = $ldap->search(base => $BASEDN, filter => '(&(objectclass=groupofnames)(cn=A*))');
compare_ldif("51c",$mesg,$mesg->sorted);

# Not searching
$mesg = $ldap->search(base => $BASEDN, filter => '(!(objectclass=person))');
compare_ldif("51d",$mesg,$mesg->sorted);

