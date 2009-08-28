#!perl

BEGIN {
  require "t/common.pl";
}

unless (eval { require IO::Socket::SSL; 1} ) {
  print "1..0 # IO::Socket::SSL not installed\n";
  exit;
}

start_server(version => 3, ssl => 1);

print "1..15\n";

$ldap = client();
ok($ldap, "client");

$mesg = $ldap->bind($MANAGERDN, password => $PASSWD, version => 3);

ok(!$mesg->code, "bind: " . $mesg->code . ": " . $mesg->error);

ok(ldif_populate($ldap, "data/50-in.ldif"), "data/50-in.ldif");

$mesg = $ldap->start_tls;
ok(!$mesg->code, "start_stl: " . $mesg->code . ": " . $mesg->error);

$mesg = $ldap->start_tls;
ok($mesg->code, "start_stl: " . $mesg->code . ": " . $mesg->error);

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

compare_ldif("50",$mesg,$mesg->sorted);

$ldap = client(ssl => 1);
ok($ldap, "ssl client");

$mesg = $ldap->start_tls;
ok($mesg->code, "start_stl: " . $mesg->code . ": " . $mesg->error);

$mesg = $ldap->search(base => $BASEDN, filter => 'objectclass=*');
ok(!$mesg->code, "search: " . $mesg->code . ": " . $mesg->error);

compare_ldif("50",$mesg,$mesg->sorted);

