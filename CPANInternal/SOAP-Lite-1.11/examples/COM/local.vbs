' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to local ASP/Daemon server

MsgBox CreateObject("SOAP.Lite").new( _
  "proxy", "http://localhost/soap.asp", _
  "uri",   "http://www.soaplite.com/My/Examples" _
).getStateName(1).result
