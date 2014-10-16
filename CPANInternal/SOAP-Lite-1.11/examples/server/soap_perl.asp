<%@Language=PerlScript%>
<%
  # -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

  # SOAP server. requires SOAP.Lite COM object (regsvr32 Lite.dll)
  # Path to Perl modules can be specified as '/PATH/TO/MODULES'

  $Response->{ContentType} = "text/xml";
  $Response->Write($Server->CreateObject("SOAP.Lite")
    -> server("SOAP::Server")
    -> dispatch_to("/Your/Path/To/Deployed/Modules")
    -> handle($Request->BinaryRead($Request->{TotalBytes}))
  );
%>
