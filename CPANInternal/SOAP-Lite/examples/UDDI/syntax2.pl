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

# NB! order of elements DOES matter (and DOES NOT for attributes)

my $busent = with businessEntity =>
  name("Contoso Manufacturing"), 
  description("We make components for business"),
  businessKey(''),
  businessServices with businessService =>
    name("Buy components"), 
    description("Bindings for buying our components"),
    serviceKey(''),
    bindingTemplates with bindingTemplate =>
      description("BASDA invoices over HTTP post"),
      accessPoint('http://www.contoso.com/buy.asp'),
      bindingKey(''),
      tModelInstanceDetails with tModelInstanceInfo =>
        description('some tModel'),
        tModelKey('UUID:C1ACF26D-9672-4404-9D70-39B756E62AB4')
;

my $newent = save_business($auth, $busent);
print $newent->businessEntity->businessKey if ref $newent;

#
# (almost) the same code from Microsoft UDDI toolkit
#
# With be.AddbusinessEntity
#     .Name = "Contoso Manufacturing"
#     .Adddescription = "We make components for business"
#     With .businessServices.AddbusinessService
#         .Name = "Buy components"
#         .Adddescription = "Bindings for buying our components"
#         With .bindingTemplates.AddbindingTemplate
#             .accessPoint = "http://www.contoso.com/buy.asp"
#             .addescription = "BASDA invoices over HTTP post"
#             With .tModelInstanceDetails.AddtModelInstanceInfo
#                 .tModelKey = GUID_BASDA_INVOICE
#             End With
#         End With
#     End With
# End With
#
