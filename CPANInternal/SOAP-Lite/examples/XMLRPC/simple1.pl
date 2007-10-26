#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use XMLRPC::Lite;

print XMLRPC::Lite
  -> proxy('http://betty.userland.com/RPC2')
  -> call('examples.getStateName', 25)
  -> result;
