' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to local ASP/Daemon server

Set hashRes = CreateObject("SOAP.Lite").new( _
  "proxy", "http://localhost/soap.asp", _
  "uri",   "http://www.soaplite.com/My/Examples" _
).getStateStruct(CreateObject("SOAP.Lite").hash("a", 1, "b", 2)) _
 .result

MsgBox hashRes.a
MsgBox hashRes.b