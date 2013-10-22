#!perl -w
#!d:\perl\bin\perl.exe 

# -- UDDI::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use strict;
use UDDI::Lite 
  import => 'UDDI::Data', 
  import => 'UDDI::Lite',
  proxy => "https://some.server.com/endpoint_fot_publishing_API",
  on_fault => sub {}, # do nothing for fault, will check it in the code
;

print "Authorizing...\n";
my $auth = get_authToken({userID => 'USERID', cred => 'CRED'})->authInfo;

# MAY also work without businessKey, serviceKey and bindingKey assignments, 
# however specification requires them to be assigned to the empty strings

# NB! order of elements DOES matter

my $busent = businessEntity([
  name("Contoso Manufacturing"), 
  description("We make components for business"),
  businessServices(
    businessService([
      name("Buy components"), 
      description("Bindings for buying our components"),
      bindingTemplates(
        bindingTemplate([
          description("BASDA invoices over HTTP post"),
          accessPoint("http://www.contoso.com/buy.asp"),
          tModelInstanceDetails(
            tModelInstanceInfo->tModelKey('UUID:C1ACF26D-9672-4404-9D70-39B756E62AB4')
          ),
        ])->bindingKey(''),
      ),
    ])->serviceKey(''),
  ),
])->businessKey('');

my $newent = save_business($auth, $busent);
print $newent->businessEntity->businessKey if ref $newent;
