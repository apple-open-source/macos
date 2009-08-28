#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite;

# real user/passwords are used to give you a chance to try it out-of-the-box
# you may put your own user/password

my $s = SOAP::Lite
  -> proxy('jabber://soaplite_client:soapliteclient@jabber.org:5222/soaplite_server@jabber.org/')
;

my $r = $s->echo('Hello, Jabber world!');
print $r->fault ? $r->faultstring : $r->result;
