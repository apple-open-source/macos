#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Transport::JABBER;

# real user/passwords are used to give you a chance to try it out-of-the-box
# you may put your own user/password

my $server = SOAP::Transport::JABBER::Server
  -> new('jabber://soaplite_server:soapliteserver@jabber.org:5222')
  -> dispatch_to('echo')
;

print "Contact to SOAP server\n";

do { $server->handle } while sleep 1;

sub echo { $_[1] }
