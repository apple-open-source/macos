#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use strict;
use UDDI::Lite 
  import => 'UDDI::Data', 
  import => 'UDDI::Lite',
  proxy => "https://some.server.com/endpoint_fot_publishing_API",
;

my $name = 'Sample business';

print "Authorizing...\n";
my $auth = get_authToken({userID => 'USERID', cred => 'CRED'})->authInfo;

my $tmodel = tModelInstanceInfo(description('some tModel'))->tModelKey('UUID:C1ACF26D-9672-4404-9D70-39B756E62AB4');
my $tmodels = tModelInstanceDetails($tmodel);
my $bindtmpl = bindingTemplate([accessPoint('someurl'), $tmodels]);
my $bindtmpls = bindingTemplates($bindtmpl);
my $bussvc = businessService([name('Test Service'), $bindtmpls]);
my $bussvcs = businessServices($bussvc);
my $busent = businessEntity([name($name), $bussvcs])->businessKey('');

print "Saving business '$name'...\n";
my $newent = save_business($auth, $busent)->businessEntity;
my $newkey = $newent->businessKey;
