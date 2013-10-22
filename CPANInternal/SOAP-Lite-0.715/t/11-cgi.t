#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use SOAP::Test;

SOAP::Test::Server::run_for(shift || 'http://localhost/cgi-bin/soap.cgi');

