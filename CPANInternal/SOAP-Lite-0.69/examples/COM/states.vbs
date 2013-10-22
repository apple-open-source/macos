' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

MsgBox CreateObject("SOAP.Lite").new( _
  "proxy", "http://localhost/", _
  "uri",   "http://www.soaplite.com/My/Examples" _
).getStateName(2).result
