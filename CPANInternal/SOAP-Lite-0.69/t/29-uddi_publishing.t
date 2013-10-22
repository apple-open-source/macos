#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

use UDDI::Lite 
  import => 'UDDI::Data',
  import => 'UDDI::Lite',
  proxy => "https://uddi.xmethods.net:8005/glue/publish/uddi";

my($a, $s, $r, $serialized, $deserialized);

# ------------------------------------------------------
use SOAP::Test;

$s = SOAP::Lite->uri('http://something/somewhere')->proxy("https://uddi.xmethods.net:8005/glue/publish/uddi")->on_fault(sub{});
eval { $s->transport->timeout($SOAP::Test::TIMEOUT = $SOAP::Test::TIMEOUT) };
$r = $s->test_connection;

unless (defined $r && defined $r->envelope) {
  print "1..0 # Skip: ", $s->transport->status, "\n"; 
  exit;
}
# ------------------------------------------------------

plan tests => 7;

{
  # You may run these tests/examples for UDDI publishing API against
  # UDDI registry that was kindly provided with following disclamer:
  # "This is a free registry provided by XMethods.net and
  # implemented using GLUE platform (Graham Glass, TheMindElectric)."
  # Thanks to Tony Hong for his help and support

  my $name = 'Sample business ' . $$ . time; # just to make it unique

  print "Authorizing...\n";
  my $auth = get_authToken({userID => 'soaplite', cred => 'soaplite'})->authInfo;
  ok(defined $auth);
  my $busent = businessEntity(name($name))->businessKey('');
  ok(defined $busent);

  print "Saving business '$name'...\n";
  my $newent = save_business($auth, $busent)->businessEntity;
  ok(UNIVERSAL::isa($newent => 'HASH'));
  my $newkey = $newent->businessKey;
  ok(length($newkey) == 36);

  ok($newent->discoveryURLs->discoveryURL =~ /$newkey/);
  print "Created...\n";
  print $newkey, "\n";
  print $newent->discoveryURLs->discoveryURL, "\n";

  print "Deleting '$newkey'...\n";
  my $result = delete_business($auth, $newkey)->result;
  ok(defined $result);

  # IBM returns 'successful' with type
  # GLUE returns long sentence that has 'no failure'
  ok($result->errInfo =~ /succ?essful|no failure/i);
  print $result->errInfo, "\n";
}
