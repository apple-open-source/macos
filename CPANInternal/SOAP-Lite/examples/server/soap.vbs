' -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

' SOAP server. requires SOAP.Lite COM object (regsvr32 Lite.dll)
' Doesn't require web server
' Path to Perl modules can be specified
' as '/PATH/TO/MODULES' or as 'drive:/PATH/TO/MODULES'

call CreateObject("SOAP.Lite") _
 .server("SOAP::Transport::HTTP::Daemon", _
   "LocalAddr", "localhost", _
   "LocalPort", 80) _
 .dispatch_to("/Your/Path/To/Deployed/Modules") _
 .options(CreateObject("SOAP.Lite").hash("compress_threshold", 10000)) _
 .handle
