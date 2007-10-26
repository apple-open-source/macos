' -- XMLRPC::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to remote XMLRPC server

MsgBox CreateObject("SOAP.Lite").xmlrpc( _
  "proxy", "http://betty.userland.com/RPC2" _
).call("examples.getStateName", 25).result
