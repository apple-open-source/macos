#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use SOAP::Lite 
  uri => 'http://www.soaplite.com/My/Examples',
  proxy => 'http://localhost/', 
# proxy => 'http://localhost/cgi-bin/soap.cgi', # local CGI server
# proxy => 'http://localhost/',                 # local daemon server
# proxy => 'http://localhost/soap',             # local mod_perl server
# proxy => 'https://localhost/soap',            # local mod_perl SECURE server
# proxy => 'tcp://localhost:82/',               # local tcp server
# proxy => 'http://login:password@localhost/cgi-bin/soap.cgi', # local CGI server with authentication

# proxy => 'jabber://user:password@server:port/to@address/', # JABBER transport

# proxy => ['local:', dispatch_to => 'My::Examples'], # LOCAL transport

# following examples are one-way only, they don't return any response
# proxy => 'ftp://login:password@ftp.somewhere.com/relative/path/to/file.xml', # FTP transport
# proxy => 'ftp://login:password@ftp.somewhere.com//absolute/path/to/file.xml', # FTP transport

# you can always pass more than one parameter for proxy
# proxy => ['mailto:destination.email@address', smtp => 'smtp.server', From => 'your.email', Subject => 'SOAP message'], # SMTP transport
;

print SOAP::Lite->new->getStateName(1)->result;
