#!/bin/env perl 

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use XMLRPC::Test;

XMLRPC::Test::Server::run_for(shift || 'http://localhost/mod_xmlrpc');

