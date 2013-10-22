#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite +autodispatch => 
  uri => 'http://www.soaplite.com/My/Parameters',
  proxy => 'http://localhost/soap',
# proxy => 'http://localhost/',                 # local daemon server
# proxy => 'http://localhost/soap',             # local mod_perl server
# proxy => 'https://localhost/soap',            # local mod_perl SECURE server
# proxy => 'tcp://localhost:82',                # local tcp server
  on_fault => sub { my($soap, $res) = @_; 
    die ref $res ? $res->faultdetail : $soap->transport->status, "\n";
  }
;

print "Session iterator\n";
my $p = My::SessionIterator->new(10);     
print $p->next, "\n";  
print $p->next, "\n";   

print "Persistent iterator\n";
$p = My::PersistentIterator->new(10);     
print $p->next, "\n";  
print $p->next, "\n";   
