#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite +autodispatch => 
  uri => 'http://www.soaplite.com/', 
  proxy => 'http://localhost/', 
# proxy => 'http://localhost/cgi-bin/soap.cgi', # local CGI server
# proxy => 'http://localhost/',                 # local daemon server
# proxy => 'http://localhost/soap',             # local mod_perl server
# proxy => 'https://localhost/soap',            # local mod_perl SECURE server
# proxy => 'tcp://localhost:82',                # local tcp server
  on_fault => sub { my($soap, $res) = @_; 
    die ref $res ? $res->faultdetail : $soap->transport->status, "\n";
  }
;

use My::PingPong;
# you can manipulate same object on remote and local machine
my $p = My::PingPong->new(10);           # local
# my $p = My::PingPong->SOAP::new(10);   # same thing remotely
print 'remote: ', $p->SOAP::next, "\n";  # remote
print 'local: ', $p->next, "\n";         # local
print 'remote: ', $p->SOAP::value, "\n"; # remote
