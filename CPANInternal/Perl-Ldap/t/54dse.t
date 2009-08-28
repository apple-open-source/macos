#!perl

BEGIN {
  require "t/common.pl";
  start_server(version => 3);
}

print "1..4\n";

$ldap = client();
ok($ldap, "client");

$dse = $ldap->root_dse;
ok($dse, "dse");

$dse->dump if $dse and $ENV{TEST_VERBOSE};

my @extn = $dse->get_value('supportedExtension');

ok($dse->supported_extension(@extn), 'supported_extension');

ok(!$dse->supported_extension('foobar'), 'extension foobar');


