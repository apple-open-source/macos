#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite +autodispatch => 
  uri => 'http://www.soaplite.com/My/Examples', 
  proxy => 'tcp://localhost:82',                  # local tcp server
  on_fault => sub { my($soap, $res) = @_; 
    die ref $res ? $res->faultdetail : $soap->transport->status, "\n";
  }
;

print getStateName(1), "\n\n";
print getStateNames(12,24,26,13), "\n\n";
print getStateList([11,12,13,42])->[0], "\n\n";
print getStateStruct({item1 => 10, item2 => 4})->{item2}, "\n\n";
