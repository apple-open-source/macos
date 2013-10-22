' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' Connect to remote SOAP server

Set soaplite = CreateObject("SOAP.Lite")
' values can be stored in hash using "hash" method
Set subhash = soaplite.hash("key_c", "Value C")
Set hash = soaplite.hash("key_a", "Value A", "key_b", subhash)
' or can be assigned directly
hash.key_b.key_d = "Value D"

Set echohash = soaplite.new( _
  "proxy", "http://services.soaplite.com/echo.cgi", _
  "uri",   "http://namespaces.soaplite.com/Echo" _
).echo(hash).result

MsgBox "key_a: "       & echohash.key_a       & chr(13) & _
       "key_b+key_c: " & echohash.key_b.key_c & chr(13) & _
       "key_b+key_d: " & echohash.key_b.key_d
