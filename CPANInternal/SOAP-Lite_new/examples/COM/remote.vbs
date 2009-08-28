' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to remote SOAP server

MsgBox CreateObject("SOAP.Lite").new( _
  "proxy", "http://services.xmethods.net/soap", _
  "uri",   "urn:xmethods-delayed-quotes" _
).getQuote("MSFT").result
