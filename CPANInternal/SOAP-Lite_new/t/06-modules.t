#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

BEGIN { plan tests => 17 }

foreach (qw(SOAP::Lite SOAP::Transport::HTTP SOAP::Transport::MAILTO
            SOAP::Transport::FTP SOAP::Transport::TCP SOAP::Transport::IO
            SOAP::Transport::LOCAL SOAP::Transport::POP3 XML::Parser::Lite
            UDDI::Lite XMLRPC::Lite XMLRPC::Transport::HTTP 
            XMLRPC::Transport::TCP XMLRPC::Transport::POP3 SOAP::Packager 
            SOAP::Transport::MQ SOAP::Transport::JABBER
            )) {
  eval "require $_";

  if ($@ =~ /(Can\'t locate)|(XML::Parser::Lite requires)|(this is only version)|(load mod_perl)/) {
    skip($@ => $@, '');
  } else {
    ok(!$@) or warn "\nError while loading $_\n";
  }
}
