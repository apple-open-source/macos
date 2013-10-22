' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to remote SOAP server

MsgBox CreateObject("SOAP.Lite").new( _
  "proxy", "http://services.soaplite.com/temper.cgi", _
  "uri",   "http://www.soaplite.com/Temperatures" _
).c2f(37.5).result
