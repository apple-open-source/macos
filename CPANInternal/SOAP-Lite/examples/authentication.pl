#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite +autodispatch => 
  uri => 'http://www.soaplite.com/My/Examples', 
  proxy => 'http://localhost/', 
# proxy => 'http://localhost/cgi-bin/soap.cgi', # local CGI server
# proxy => 'http://localhost/soap',             # local mod_perl server
# proxy => 'https://localhost/soap',            # local mod_perl SECURE server
  on_fault => sub { my($soap, $res) = @_; 
    die ref $res ? $res->faultdetail : $soap->transport->status, "\n";
  }
;
sub SOAP::Transport::HTTP::Client::get_basic_credentials { return ('user' => 'password') };

print getStateName(1), "\n\n";
print getStateNames(12,24,26,13), "\n\n";
print getStateList([11,12,13,42])->[0], "\n\n";
print getStateStruct({item1 => 10, item2 => 4})->{item2}, "\n\n";

# OR if you have SOAP::Lite object you can do
# $s->transport->credentials('host_port', 'realm', 'user' => 'password');

# see LWP::UserAgent for difference and more documentation

# OR add user and password to your URL as follows:
# proxy => 'http://user:password@localhost/'
