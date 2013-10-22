#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Transport::HTTP;

SOAP::Transport::HTTP::CGI   
  -> dispatch_to('Style')     
  -> handle;

package Style; 

sub echo { 
  return SOAP::Data->name(echo => $_[1]),
         SOAP::Header->name('html:style' => <<EOS)->uri('http://www.w3.org/Profiles/XHTML-transitional');
echo { display: block; color: red; }
EOS
}
