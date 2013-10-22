# ======================================================================
#
# Copyright (C) 2000-2004 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: HTTP.pm,v 1.1 2004/10/16 17:45:17 byrnereese Exp $
#
# ======================================================================

__END__

=head1 NAME

SOAP::Transport::HTTP - Server/Client side HTTP support for SOAP::Lite

=head1 SYNOPSIS

=over 4

=item Client

  use SOAP::Lite 
    uri => 'http://my.own.site.com/My/Examples',
    proxy => 'http://localhost/', 
  # proxy => 'http://localhost/cgi-bin/soap.cgi', # local CGI server
  # proxy => 'http://localhost/',                 # local daemon server
  # proxy => 'http://localhost/soap',             # local mod_perl server
  # proxy => 'https://localhost/soap',            # local mod_perl SECURE server
  # proxy => 'http://login:password@localhost/cgi-bin/soap.cgi', # local CGI server with authentication
  ;

  print getStateName(1);

=item CGI server

  use SOAP::Transport::HTTP;

  SOAP::Transport::HTTP::CGI
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
    -> handle
  ;

=item Daemon server

  use SOAP::Transport::HTTP;

  # change LocalPort to 81 if you want to test it with soapmark.pl

  my $daemon = SOAP::Transport::HTTP::Daemon
    -> new (LocalAddr => 'localhost', LocalPort => 80)
    # specify list of objects-by-reference here 
    -> objects_by_reference(qw(My::PersistentIterator My::SessionIterator My::Chat))
    # specify path to My/Examples.pm here
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
  ;
  print "Contact to SOAP server at ", $daemon->url, "\n";
  $daemon->handle;

=item Apache mod_perl server

See F<examples/server/Apache.pm> and L</"EXAMPLES"> section for more information.

=item mod_soap server (.htaccess, directory-based access)

  SetHandler perl-script
  PerlHandler Apache::SOAP
  PerlSetVar dispatch_to "/Your/Path/To/Deployed/Modules, Module::Name, Module::method"
  PerlSetVar options "compress_threshold => 10000"

See L<Apache::SOAP> for more information.

=back

=head1 DESCRIPTION

This class encapsulates all HTTP related logic for a SOAP server,
independent of what web server it's attached to. 
If you want to use this class you should follow simple guideline
mentioned above. 

Following methods are available:

=over 4

=item on_action()

on_action method lets you specify SOAPAction understanding. It accepts
reference to subroutine that takes three parameters: 

  SOAPAction, method_uri and method_name. 

C<SOAPAction> is taken from HTTP header and method_uri and method_name are 
extracted from request's body. Default behavior is match C<SOAPAction> if 
present and ignore it otherwise. You can specify you own, for example 
die if C<SOAPAction> doesn't match with following code:

  $server->on_action(sub {
    (my $action = shift) =~ s/^("?)(.+)\1$/$2/;
    die "SOAPAction shall match 'uri#method'\n" if $action ne join '#', @_;
  });

=item dispatch_to()

dispatch_to lets you specify where you want to dispatch your services 
to. More precisely, you can specify C<PATH>, C<MODULE>, C<method> or 
combination C<MODULE::method>. Example:

  dispatch_to( 
    'PATH/',          # dynamic: load anything from there, any module, any method
    'MODULE',         # static: any method from this module 
    'MODULE::method', # static: specified method from this module
    'method',         # static: specified method from main:: 
  );

If you specify C<PATH/> name of module/classes will be taken from uri as 
path component and converted to Perl module name with substitution 
'::' for '/'. Example:

  urn:My/Examples              => My::Examples
  urn://localhost/My/Examples  => My::Examples
  http://localhost/My/Examples => My::Examples

For consistency first '/' in the path will be ignored.

According to this scheme to deploy new class you should put this
class in one of the specified directories and enjoy its services.
Easy, eh? 

=item handle()

handle method will handle your request. You should provide parameters
with request() method, call handle() and get it back with response() .

=item request()

request method gives you access to HTTP::Request object which you
can provide for Server component to handle request.

=item response()

response method gives you access to HTTP::Response object which 
you can access to get results from Server component after request was
handled.

=back

=head2 PROXY SETTINGS

You can use any proxy setting you use with LWP::UserAgent modules:

 SOAP::Lite->proxy('http://endpoint.server/', 
                   proxy => ['http' => 'http://my.proxy.server']);

or

 $soap->transport->proxy('http' => 'http://my.proxy.server');

should specify proxy server for you. And if you use C<HTTP_proxy_user> 
and C<HTTP_proxy_pass> for proxy authorization SOAP::Lite should know 
how to handle it properly. 

=head2 COOKIE-BASED AUTHENTICATION

  use HTTP::Cookies;

  my $cookies = HTTP::Cookies->new(ignore_discard => 1);
    # you may also add 'file' if you want to keep them between sessions

  my $soap = SOAP::Lite->proxy('http://localhost/');
  $soap->transport->cookie_jar($cookies);

Cookies will be taken from response and provided for request. You may
always add another cookie (or extract what you need after response)
with HTTP::Cookies interface.

You may also do it in one line:

  $soap->proxy('http://localhost/', 
               cookie_jar => HTTP::Cookies->new(ignore_discard => 1));

=head2 SSL CERTIFICATE AUTHENTICATION

To get certificate authentication working you need to specify three
environment variables: C<HTTPS_CERT_FILE>, C<HTTPS_KEY_FILE>, and 
(optionally) C<HTTPS_CERT_PASS>:

  $ENV{HTTPS_CERT_FILE} = 'client-cert.pem';
  $ENV{HTTPS_KEY_FILE}  = 'client-key.pem';

Crypt::SSLeay (which is used for https support) will take care about 
everything else. Other options (like CA peer verification) can be specified
in a similar way. See Crypt::SSLeay documentation for more details.

Those who would like to use encrypted keys may check 
http://groups.yahoo.com/group/soaplite/message/729 for details. 

=head2 COMPRESSION

SOAP::Lite provides you with the option for enabling compression on the 
wire (for HTTP transport only). Both server and client should support 
this capability, but this should be absolutely transparent to your 
application. The Server will respond with an encoded message only if 
the client can accept it (indicated by client sending an Accept-Encoding 
header with 'deflate' or '*' values) and client has fallback logic, 
so if server doesn't understand specified encoding 
(Content-Encoding: deflate) and returns proper error code 
(415 NOT ACCEPTABLE) client will repeat the same request without encoding
and will store this server in a per-session cache, so all other requests 
will go there without encoding.

Having options on client and server side that let you specify threshold
for compression you can safely enable this feature on both client and 
server side.

=over 4

=item Client

  print SOAP::Lite
    -> uri('http://localhost/My/Parameters')
    -> proxy('http://localhost/', options => {compress_threshold => 10000})
    -> echo(1 x 10000)
    -> result
  ;

=item Server

  my $server = SOAP::Transport::HTTP::CGI
    -> dispatch_to('My::Parameters')
    -> options({compress_threshold => 10000})
    -> handle;

=back

Compression will be enabled on the client side 
B<if> the threshold is specified 
B<and> the size of current message is bigger than the threshold 
B<and> the module Compress::Zlib is available. 

The Client will send the header 'Accept-Encoding' with value 'deflate'
B<if> the threshold is specified 
B<and> the module Compress::Zlib is available.

Server will accept the compressed message if the module Compress::Zlib 
is available, and will respond with the compressed message 
B<only if> the threshold is specified 
B<and> the size of the current message is bigger than the threshold 
B<and> the module Compress::Zlib is available 
B<and> the header 'Accept-Encoding' is presented in the request.

=head1 EXAMPLES

Consider following examples of SOAP servers:

=over 4

=item CGI:

  use SOAP::Transport::HTTP;

  SOAP::Transport::HTTP::CGI
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
    -> handle
  ;

=item daemon:

  use SOAP::Transport::HTTP;

  my $daemon = SOAP::Transport::HTTP::Daemon
    -> new (LocalAddr => 'localhost', LocalPort => 80)
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
  ;
  print "Contact to SOAP server at ", $daemon->url, "\n";
  $daemon->handle;

=item mod_perl:

httpd.conf:

  <Location /soap>
    SetHandler perl-script
    PerlHandler SOAP::Apache
  </Location>

Apache.pm:

  package SOAP::Apache;

  use SOAP::Transport::HTTP;

  my $server = SOAP::Transport::HTTP::Apache
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method'); 

  sub handler { $server->handler(@_) }

  1;

=item Apache::Registry:

httpd.conf:

  Alias /mod_perl/ "/Apache/mod_perl/"
  <Location /mod_perl>
    SetHandler perl-script
    PerlHandler Apache::Registry
    PerlSendHeader On
    Options +ExecCGI
  </Location>

soap.mod_cgi (put it in /Apache/mod_perl/ directory mentioned above)

  use SOAP::Transport::HTTP;

  SOAP::Transport::HTTP::CGI
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
    -> handle
  ;

=back

WARNING: dynamic deployment with Apache::Registry will fail, because 
module will be loaded dynamically only for the first time. After that 
it is already in the memory, that will bypass dynamic deployment and 
produces error about denied access. Specify both PATH/ and MODULE name 
in dispatch_to() and module will be loaded dynamically and then will work 
as under static deployment. See examples/server/soap.mod_cgi for example.

=head1 TROUBLESHOOTING

=over 4

=item Dynamic libraries are not found

If you see in webserver's log file something like this: 

Can't load '/usr/local/lib/perl5/site_perl/.../XML/Parser/Expat/Expat.so' 
for module XML::Parser::Expat: dynamic linker: /usr/local/bin/perl:
 libexpat.so.0 is NEEDED, but object does not exist at
/usr/local/lib/perl5/.../DynaLoader.pm line 200.

and you are using Apache web server, try to put into your httpd.conf

 <IfModule mod_env.c>
     PassEnv LD_LIBRARY_PATH
 </IfModule>

=item Apache is crashing with segfaults (it may looks like "500 unexpected EOF before status line seen" on client side)

If using SOAP::Lite (or XML::Parser::Expat) in combination with mod_perl
causes random segmentation faults in httpd processes try to configure
Apache with:

 RULE_EXPAT=no

-- OR (for Apache 1.3.20 and later) --

 ./configure --disable-rule=EXPAT

See http://archive.covalent.net/modperl/2000/04/0185.xml for more 
details and lot of thanks to Robert Barta <rho@bigpond.net.au> for
explaining this weird behavior.

If it doesn't help, you may also try -Uusemymalloc
(or something like that) to get perl to use the system's own malloc.
Thanks to Tim Bunce <Tim.Bunce@pobox.com>.

=item CGI scripts are not running under Microsoft Internet Information Server (IIS)

CGI scripts may not work under IIS unless scripts are .pl, not .cgi.

=back

=head1 DEPENDENCIES

 Crypt::SSLeay             for HTTPS/SSL
 SOAP::Lite, URI           for SOAP::Transport::HTTP::Server
 LWP::UserAgent, URI       for SOAP::Transport::HTTP::Client
 HTTP::Daemon              for SOAP::Transport::HTTP::Daemon
 Apache, Apache::Constants for SOAP::Transport::HTTP::Apache

=head1 SEE ALSO

 See ::CGI, ::Daemon and ::Apache for implementation details.
 See examples/server/soap.cgi as SOAP::Transport::HTTP::CGI example.
 See examples/server/soap.daemon as SOAP::Transport::HTTP::Daemon example.
 See examples/My/Apache.pm as SOAP::Transport::HTTP::Apache example.

=head1 COPYRIGHT

Copyright (C) 2000-2001 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHOR

Paul Kulchenko (paulclinger@yahoo.com)

=cut
