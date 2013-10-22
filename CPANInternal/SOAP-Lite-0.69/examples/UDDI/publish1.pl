#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

# You may run these tests/examples for UDDI publishing API against
# UDDI registry that was kindly provided with following disclamer:
# "This is a free registry provided by XMethods.net and
# implemented using GLUE platform (Graham Glass, TheMindElectric)."
# Thanks to Tony Hong for his help and support

use strict;
use UDDI::Lite 
  import => 'UDDI::Data', 
  import => 'UDDI::Lite',
  proxy => "https://uddi.xmethods.net:8005/glue/publish/uddi",
;

my $name = 'Sample business ' . $$ . time; # just to make it unique

print "Authorizing...\n";
my $auth = get_authToken({userID => 'soaplite', cred => 'soaplite'})->authInfo;
my $busent = businessEntity(name($name))->businessKey('');

print "Saving business '$name'...\n";
my $newent = save_business($auth, $busent)->businessEntity;
my $newkey = $newent->businessKey;

print "Created...\n";
print $newkey, "\n";
print $newent->discoveryURLs->discoveryURL, "\n";

print "Deleting '$newkey'...\n";
my $result = delete_business($auth, $newkey)->result;

print $result->errInfo, "\n";
