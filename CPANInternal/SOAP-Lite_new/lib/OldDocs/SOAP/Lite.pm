# ======================================================================
#
# Copyright (C) 2000-2003 Paul Kulchenko (paulclinger@yahoo.com)
# SOAP::Lite is free software; you can redistribute it
# and/or modify it under the same terms as Perl itself.
#
# $Id: Lite.pm 95 2007-10-09 09:10:44Z kutterma $
#
# ======================================================================

=pod

=head1 NAME

SOAP::Lite - Client and server side SOAP implementation

=head1 SYNOPSIS

  use SOAP::Lite;
  print SOAP::Lite
    -> uri('http://www.soaplite.com/Temperatures')
    -> proxy('http://services.soaplite.com/temper.cgi')
    -> f2c(32)
    -> result;

The same code with autodispatch: 

  use SOAP::Lite +autodispatch =>
    uri => 'http://www.soaplite.com/Temperatures',
    proxy => 'http://services.soaplite.com/temper.cgi';

  print f2c(32);

Code in OO-style:

  use SOAP::Lite +autodispatch =>
    uri => 'http://www.soaplite.com/Temperatures',
    proxy => 'http://services.soaplite.com/temper.cgi';

  my $temperatures = Temperatures->new(32); # get object
  print $temperatures->as_celsius;          # invoke method

Code with service description:

  use SOAP::Lite;
  print SOAP::Lite
    -> service('http://www.xmethods.net/sd/StockQuoteService.wsdl')
    -> getQuote('MSFT');

Code for SOAP server (CGI):

  use SOAP::Transport::HTTP;
  SOAP::Transport::HTTP::CGI
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'Module::Name', 'Module::method') 
    -> handle;

Visual Basic client (through COM interface): 

  MsgBox CreateObject("SOAP.Lite").new( _
    "proxy", "http://services.xmethods.net/soap", _
    "uri",   "urn:xmethods-delayed-quotes" _
  ).getQuote("MSFT").result

mod_soap enabled SOAP server: 

  .htaccess

  SetHandler perl-script
  PerlHandler Apache::SOAP
  PerlSetVar dispatch_to "/Your/Path/To/Deployed/Modules, Module::Name"

ASP/VB SOAP server: 

  <%
    Response.ContentType = "text/xml"
    Response.Write(Server.CreateObject("SOAP.Lite") _
      .server("SOAP::Server") _ 
      .dispatch_to("/Your/Path/To/Deployed/Modules") _
      .handle(Request.BinaryRead(Request.TotalBytes)) _
    )
  %>

=head1 DESCRIPTION

SOAP::Lite is a collection of Perl modules which provides a 
simple and lightweight interface to the Simple Object Access Protocol 
(SOAP) both on client and server side.

This version of SOAP::Lite supports the SOAP 1.1 specification ( http://www.w3.org/TR/SOAP ).

The main features of the library are:

=over 3

=item *

Supports SOAP 1.1 spec. 

=item *

Interoperability tests with different implementations: Apache SOAP, Apache Axis, Frontier, Microsoft SOAP, Microsoft .NET, DevelopMentor, XMethods, 4s4c, Phalanx, PocketSOAP, Kafka, SQLData, Lucin (in Java, Perl, C++, Python, VB, COM, XSLT). 

=item *

Provides COM interface. Single dll (standalone [2.5MB] or minimal [32kB]).
Works on Windows 9x/Me/NT/2K. Doesn't require ROPE or MSXML.
Examples in VB, Excel/VBA, C#, ASP, JavaScript, PerlScript and Perl.

=item *

Provides transparent compression support for HTTP transport. 

=item *

Provides mod_soap module. Make SOAP server with a few lines in .htaccess 
or .conf file. 

=item *

Includes XML::Parser::Lite (regexp-based XML parser) which runs instead 
of XML::Parser where Perl 5.6 runs (even on WinCE) with some limitations. 

=item *

Includes XMLRPC::Lite, implementation of XML-RPC protocol on client and 
server side. All transports and features of SOAP::Lite are available. 

=item *

Supports multipart/form-data MIME attachments. 

=item *

Supports circular linked lists and multiple references. 

=item *

Supports Map datatype (encoding of maps/hashes with arbitrary keys). 

=item *

Supports HTTPS protocol. 

=item *

Provides proxy support. 

=item *

Provides CGI/daemon/mod_perl/Apache::Registry server implementations. 

=item *

Provides TCP server implementation. 

=item *

Provides IO (STDIN/STDOUT/File) server implementation. 

=item *

Provides FTP client implementation. 

=item *

Supports single/multipart MIME attachment (parsing side only). 

=item *

Supports SMTP protocol. 

=item *

Provides POP3 server implementation. 

=item *

Supports M-POST and redirects in HTTP transport. 

=item *

Supports Basic/Digest server authentication. 

=item *

Works with CGI accelerators, like VelociGen and PerlEx. 

=item *

Supports UDDI interface on client side. See UDDI::Lite for details. 

=item *

Supports UDDI publishing API. Examples and documentation provided. 

=item *

Supports WSDL schema with stub and run-time access. 

=item *

Supports blessed object references. 

=item *

Supports arrays (both serialization and deserialization with autotyping). 

=item *

Supports custom serialization. 

=item *

Provides exception transport with custom exceptions 

=item *

Supports Base64 encoding. 

=item *

Supports XML entity encoding. 

=item *

Supports header attributes. 

=item *

Supports dynamic and static class/method binding. 

=item *

Supports objects-by-reference with simple garbage collection and activation. 

=item *

Provides shell for interactive SOAP sessions. 

=item *

Supports out parameters binding. 

=item *

Supports transparent SOAP calls with autodispatch feature. 

=item *

Provides easy services deployment. Put module in specified directory and 
it'll be accessible. 

=item *

Has tests, examples and documentation to let you be up and running in no time.

=back

=head2 WHERE TO FIND EXAMPLES

See F<t/*.t>, F<examples/*.pl> and the module documentation for a client-side 
examples that demonstrate the serialization of a SOAP request, sending it 
via HTTP to the server and receiving the response, and the deserialization 
of the response. See F<examples/server/*> for server-side implementations.

=head1 OVERVIEW OF CLASSES AND PACKAGES

This table should give you a quick overview of the classes provided by the
library.

 SOAP::Lite.pm
 -- SOAP::Lite           -- Main class provides all logic
 -- SOAP::Transport      -- Supports transport architecture
 -- SOAP::Data           -- Provides extensions for serialization architecture
 -- SOAP::Header         -- Provides extensions for header serialization
 -- SOAP::Parser         -- Parses XML file into object tree
 -- SOAP::Serializer     -- Serializes data structures to SOAP package
 -- SOAP::Deserializer   -- Deserializes results of SOAP::Parser into objects
 -- SOAP::SOM            -- Provides access to deserialized object tree
 -- SOAP::Constants      -- Provides access to common constants
 -- SOAP::Trace          -- Provides tracing facilities
 -- SOAP::Schema         -- Provides access and stub(s) for schema(s)
 -- SOAP::Schema::WSDL   -- WSDL implementation for SOAP::Schema
 -- SOAP::Server         -- Handles requests on server side 
 -- SOAP::Server::Object -- Handles objects-by-reference 
 -- SOAP::Fault          -- Provides support for Faults on server side
 -- SOAP::Utils          -- A set of private and public utility subroutines

 SOAP::Transport::HTTP.pm
 -- SOAP::Transport::HTTP::Client  -- Client interface to HTTP transport
 -- SOAP::Transport::HTTP::Server  -- Server interface to HTTP transport
 -- SOAP::Transport::HTTP::CGI     -- CGI implementation of server interface
 -- SOAP::Transport::HTTP::Daemon  -- Daemon implementation of server interface
 -- SOAP::Transport::HTTP::Apache  -- mod_perl implementation of server interface

 SOAP::Transport::POP3.pm
 -- SOAP::Transport::POP3::Server  -- Server interface to POP3 protocol

 SOAP::Transport::MAILTO.pm
 -- SOAP::Transport::MAILTO::Client -- Client interface to SMTP/sendmail

 SOAP::Transport::LOCAL.pm
 -- SOAP::Transport::LOCAL::Client -- Client interface to local transport

 SOAP::Transport::TCP.pm
 -- SOAP::Transport::TCP::Server -- Server interface to TCP protocol
 -- SOAP::Transport::TCP::Client -- Client interface to TCP protocol

 SOAP::Transport::IO.pm
 -- SOAP::Transport::IO::Server -- Server interface to IO transport

=head2 SOAP::Lite

All methods that C<SOAP::Lite> provides can be used for both
setting and retrieving values. If you provide no parameters, you will
get current value, and if parameters are provided, a new value
will be assigned to the object and the method in question will return 
the current object (if not stated otherwise). This is suitable for stacking
these calls like:

  $lite = SOAP::Lite
    -> uri('http://simon.fell.com/calc')
    -> proxy('http://soap.4s4c.com/ssss4c/soap.asp')
  ;

The order is insignificant and you may call the new() method first. If you
don't do it, SOAP::Lite will do it for you. However, the new() method
gives you an additional syntax:

  $lite = new SOAP::Lite
    uri => 'http://simon.fell.com/calc',
    proxy => 'http://soap.4s4c.com/ssss4c/soap.asp'
  ;

=over 4

=item new()

new() accepts a hash with method names as keys. It will call the 
appropriate methods together with the passed values. Since new() is 
optional it won't be mentioned anymore.

=item transport()

Provides access to the L</"SOAP::Transport"> object. The object will be created 
for you. You can reassign it (but generally you should not).

=item serializer()

Provides access to the L</"SOAP::Serializer"> object. The object will be 
created for you. You can reassign it (but generally you should not).

=item proxy()

Shortcut for C<< transport->proxy() >>. This lets you specify an endpoint 
(service address) and also loads the required module at the same time. It is 
required for dispatching SOAP calls. The name of the module will be defined 
depending on the protocol specific for the endpoint. The prefix 
C<SOAP::Transport> will be prepended, the module will be loaded and object of 
class (with appended C<::Client>) will be created. 

For example, for F<http://localhost/>, the class for creating objects will 
look for C<SOAP::Transport:HTTP::Client>;

In addition to endpoint parameter, proxy() can accept any transport specific
parameters that could be passed as name => value pairs. For example, to 
specify proxy settings for HTTP protocol you may do:

  $soap->proxy('http://endpoint.server/', 
               proxy => ['http' => 'http://my.proxy.server/']);

Notice that since proxy (second one) expects to get more than one 
parameter you should wrap them in array.

Another useful example can be the client that is sensitive to cookie-based
authentication. You can provide this with:

  $soap->proxy('http://localhost/', 
               cookie_jar => HTTP::Cookies->new(ignore_discard => 1));

You may specify timeout for HTTP transport with following code:

  $soap->proxy('http://localhost/', timeout => 5);

=item endpoint()

Lets you specify an endpoint B<without> changing/loading the protocol module. 
This is useful for switching endpoints without switching protocols. You should 
call C<proxy()> first. No checks for protocol equivalence will be made.

=item outputxml()

Lets you specify the kind of output from all method calls. If C<true>, all 
methods will return unprocessed, raw XML code. You can parse it with 
XML::Parser, SOAP::Deserializer or any other appropriate module.

=item autotype()

Shortcut for C<< serializer->autotype() >>. This lets you specify whether 
the serializer will try to make autotyping for you or not. Default setting 
is C<true>.

=item readable()

Shortcut for C<< serializer->readable() >>. This lets you specify the format 
for the generated XML code. Carriage returns <CR> and indentation will be 
added for readability. Useful in the case you want to see the generated code 
in a debugger. By default, there are no additional characters in generated 
XML code. 

=item use_prefix()

Shortcut for C<< serializer->use_prefix() >>. This lets you turn on/off the
use of a namespace prefix for the children of the /Envelope/Body element.
Default is 'true'. (This was introduced in 0.61 for better .NET compatibility)

When use_prefix is set to 'true', serialized XML will look like this:

  <SOAP-ENV:Envelope ...attributes skipped>
    <SOAP-ENV:Body>
      <namesp1:mymethod xmlns:namesp1="urn:MyURI" />
    </SOAP-ENV:Body>
  </SOAP-ENV:Envelope>

When use_prefix is set to 'false', serialized XML will look like this:

  <SOAP-ENV:Envelope ...attributes skipped>
    <SOAP-ENV:Body>
      <mymethod xmlns="urn:MyURI" />
    </SOAP-ENV:Body>
  </SOAP-ENV:Envelope>

=item namespace()

Shortcut for C<< serializer->namespace() >>. This lets you specify the default
namespace for generated envelopes (C<'SOAP-ENV'> by default).

=item encodingspace()

Shortcut for C<< serializer->encodingspace() >>. This lets you specify the 
default encoding namespace for generated envelopes (C<'SOAP-ENC'> by default).

=item encoding()

Shortcut for C<< serializer->encoding() >>. This lets you specify the encoding 
for generated envelopes. It does not actually change envelope
encoding, it will just modify the XML declaration (C<'UTF-8'> by default).
Use C<undef> value to B<not> generate XML declaration.

=item typelookup()

Shortcut for C<< serializer->typelookup() >>. This gives you access to 
the C<typelookup> table that is used for autotyping. For more information
see L</"SOAP::Serializer">.

=item uri()

Shortcut for C<< serializer->uri() >>. This lets you specify the uri for SOAP 
methods. Nothing is specified by default and your call will definitely fail 
if you don't specify the required uri. 

B<WARNING>: URIs are just identifiers. They may B<look like URLs>, but they are
not guaranteed to point to anywhere and shouldn't be used as such pointers.
URIs assume to be unique within the space of all XML documents, so consider
them as unique identifiers and nothing else.

=item multirefinplace()

Shortcut for C<< serializer->multirefinplace() >>. If true, the serializer will
put values for multireferences in the first occurrence of the reference. 
Otherwise it will be encoded as top independent element, right after C<method>
element inside C<Body>. Default value is C<false>. 

=item header() 

B<DEPRECATED>: Use SOAP::Header instead. 

Shortcut for C<< serializer->header() >>. This lets you specify the header for 
generated envelopes. You can specify C<root>, C<mustUnderstand> or any
other header using L</"SOAP::Data"> class:

  $serializer = SOAP::Serializer->envelope('method' => 'mymethod', 1,
    SOAP::Header->name(t1 => 5)->mustUnderstand(1),
    SOAP::Header->name(t2 => 7)->mustUnderstand(2),
  );

will be serialized into:

  <SOAP-ENV:Envelope ...attributes skipped>
    <SOAP-ENV:Header>
      <t1 xsi:type="xsd:int" SOAP-ENV:mustUnderstand="1">5</t1>
      <t2 xsi:type="xsd:int" SOAP-ENV:mustUnderstand="1">7</t2>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
      <namesp1:mymethod xmlns:namesp1="urn:SOAP__Serializer">
        <c-gensym6 xsi:type="xsd:int">1</c-gensym6>
      </namesp1:mymethod>
    </SOAP-ENV:Body>
  </SOAP-ENV:Envelope>

You can mix C<SOAP::Header> parameters with other parameters and you can also
return C<SOAP::Header> parameters as a result of a remote call. They will be 
placed into the header. See C<My::Parameters::addheader> as an example.

=item on_action()

This lets you specify a handler for C<on_action event>. It is triggered when 
creating SOAPAction. The default handler will set SOAPAction to 
C<"uri#method">. You can change this behavior globally 
(see L</"DEFAULT SETTINGS">) or locally, for a particular object.

=item on_fault()

This lets you specify a handler for C<on_fault> event. The default behavior is 
to B<die> on an transport error and to B<do nothing> on other error conditions. You 
may change this behavior globally (see L</"DEFAULT SETTINGS">) or locally, for a 
particular object.

=item on_debug()

This lets you specify a handler for C<on_debug event>. Default behavior is to 
do nothing. Use C<+trace/+debug> option for SOAP::Lite instead. If you use if 
be warned that since this method is just interface to C<+trace/+debug> it has
B<global> effect, so if you install it for one object it'll be in effect for 
all subsequent calls (even for other objects).

See also: L<SOAP::Trace>;

=item on_nonserialized()

This lets you specify a handler for C<on_nonserialized event>. The default 
behavior is to produce a warning if warnings are on for everything that cannot 
be properly serialized (like CODE references or GLOBs).

=item call()

Provides alternative interface for remote method calls. You can always
run C<< SOAP::Lite->new(...)->method(@parameters) >>, but call() gives
you several additional options:

=over

=item prefixed method

If you want to specify prefix for generated method's element one of the
available options is do it with call() interface:

  print SOAP::Lite
    -> new(....)
    -> call('myprefix:method' => @parameters)
    -> result;

This example will work on client side only. If you want to change prefix
on server side you should override default serializer. See 
F<examples/server/soap.*> for examples. 

=item access to any method

If for some reason you want to get access to remote procedures that have 
the same name as methods of SOAP::Lite object these calls (obviously) won't 
be dispatched. In that case you can originate your call trough call():

  print SOAP::Lite
    -> new(....)
    -> call(new => @parameters) 
    -> result;

=item implementation of OO interface

With L<autodispatch|/"AUTODISPATCHING AND SOAP:: PREFIX"> you can make CLASS/OBJECT calls like:

  my $obj = CLASS->new(@parameters);
  print $obj->method;

However, because of side effects L<autodispatch|/"AUTODISPATCHING AND SOAP:: PREFIX"> 
has, it's not always possible to use this syntax. call() provides you with
alternative:

  # you should specify uri()
  my $soap = SOAP::Lite
    -> uri('http://my.own.site/CLASS') # <<< CLASS goes here
    # ..... other parameters
  ;

  my $obj = $soap->call(new => @parameters)->result;
  print $soap->call(method => $obj)->result;
  # $obj object will be updated here if necessary, 
  # as if you call $obj->method() and method() updates $obj

  # Update of modified object MAY not work if server on another side 
  # is not SOAP::Lite

=item ability to set method's attributes

Additionally this syntax lets you specify attributes for method element:

  print SOAP::Lite
    -> new(....)
    -> call(SOAP::Data->name('method')->attr({xmlns => 'mynamespace'})
            => @parameters)
    -> result;

You can specify B<any> attibutes and C<name> of C<SOAP::Data> element becomes
name of method. Everything else except attributes is ignored and parameters
should be provided as usual.

Be warned, that though you have more control using this method, you B<should> 
specify namespace attribute for method explicitely, even if you made uri() 
call earlier. So, if you have to have namespace on method element, instead of:

  print SOAP::Lite
    -> new(....)
    -> uri('mynamespace') # will be ignored 
    -> call(SOAP::Data->name('method') => @parameters)
    -> result;

do

  print SOAP::Lite
    -> new(....)
    -> call(SOAP::Data->name('method')->attr({xmlns => 'mynamespace'})
            => @parameters)
    -> result;

because in the former call uri() will be ignored and namespace won't be 
specified. If you run script with C<-w> option (as recommended) SOAP::Lite
gives you a warning:

  URI is not provided as attribute for method (method)

Moreover, it'll become fatal error if you try to call it with prefixed name:

  print SOAP::Lite
    -> new(....)
    -> uri('mynamespace') # will be ignored 
    -> call(SOAP::Data->name('a:method') => @parameters)
    -> result;

gives you:

  Can't find namespace for method (a:method)

because nothing is associated with prefix C<'a'>. 

=back

One more comment. One case when SOAP::Lite will change something that 
you specified is when you specified prefixed name and empty namespace name:

  print SOAP::Lite
    -> new(....)
    -> uri('') 
    -> call('a:method' => @parameters)
    -> result;

This code will generate:

  <method xmlns="">....</method>

instead of 

  <a:method xmlns:a="">....</method>

because later is not allowed according to XML Namespace specification.

In all other aspects C<< ->call(mymethod => @parameters) >> is just a 
synonim for C<< ->mymethod(@parameters) >>.

=item self()

Returns object reference to B<global> defaul object specified with 
C<use SOAP::Lite ...> interface. Both class method and object method return
reference to B<global> object, so:

  use SOAP::Lite
    proxy => 'http://my.global.server'
  ;

  my $soap = SOAP::Lite->proxy('http://my.local.server');

  print $soap->self->proxy;

prints C<'http://my.global.server'> (the same as C<< SOAP::Lite->self->proxy >>). 
See L</"DEFAULT SETTINGS"> for more information.

=item dispatch_from()

Does exactly the same as L<autodispatch|/"AUTODISPATCHING AND SOAP:: PREFIX">
does, but doesn't install UNIVERSAL::AUTOLOAD handler and only install
AUTOLOAD handlers in specified classes. Can be used only with C<use SOAP::Lite ...>
clause and should be specified first:

  use SOAP::Lite 
    dispatch_from => ['A', 'B'], # use "dispatch_from => 'A'" for one class
    uri => ....,
    proxy => ....,
  ;

  A->a;
  B->b;

=back

=head2 SOAP::Header

The SOAP::Header class is a subclass of the L</"SOAP::Data"> class. It is used
in the same way as a SOAP::Data object, however SOAP::Lite will serialize
objects of this type into the SOAP Envelope's Header block.

=head2 SOAP::Data

You can use this class if you want to specify a value, a name, atype, a uri or 
attributes for SOAP elements (use C<value()>, C<name()>, C<type()>, 
C<uri()> and C<attr()> methods correspondingly). 
For example, C<< SOAP::Data->name('abc')->value(123) >> will be serialized
into C<< <abc>123</abc> >>, as well as will C<< SOAP::Data->name(abc => 123) >>.
Each of them (except the value() method) can accept a value as the second 
parameter. All methods return the current value if you call them without 
parameters. The return the object otherwise, so you can stack them. See tests 
for more examples. You can import these methods with: 

  SOAP::Data->import('name'); 

or 

  import SOAP::Data 'name'; 

and then use C<< name(abc => 123) >> for brevity. 

An interface for specific attributes is also provided. You can use the C<actor()>,
C<mustUnderstand()>, C<encodingStyle()> and C<root()> methods to set/get
values of the correspondent attributes.

  SOAP::Data
    ->name(c => 3)
    ->encodingStyle('http://xml.apache.org/xml-soap/literalxml')

will be serialized into:

  <c SOAP-ENV:encodingStyle="http://xml.apache.org/xml-soap/literalxml"
     xsi:type="xsd:int">3</c>

=head2 SOAP::Serializer

Usually you don't need to interact directly with this module. The only 
case when you need it, it when using autotyping. This feature lets you specify 
types for your data according to your needs as well as to introduce new
data types (like ordered hash for example). 

You can specify a type with C<< SOAP::Data->type(float => 123) >>. During
the serialization stage the module will try to serialize your data with the 
C<as_float> method. It then calls the C<typecast> method (you can override it 
or inherit your own class from L</"SOAP::Data">) and only then it will try to 
serialize it according to data type (C<SCALAR>, C<ARRAY> or C<HASH>). For example:

  SOAP::Data->type('ordered_hash' => [a => 1, b => 2]) 

will be serialized as an ordered hash, using the C<as_ordered_hash> method.

If you do not specify a type directly, the serialization module will try
to autodefine the type for you according to the C<typelookup> hash. It contains 
the type name as key and the following 3-element array as value:

  priority, 
  check_function (CODE reference), 
  typecast function (METHOD name or CODE reference)

For example, if you want to add C<uriReference> to autodefined types,
you should add something like this:

  $s->typelookup->{uriReference} =
    [11, sub { $_[0] =~ m!^http://! }, 'as_uriReference'];

and add the C<as_uriReference> method to the L</"SOAP::Serializer"> class:

  sub SOAP::Serializer::as_uriReference {
    my $self = shift;
    my($value, $name, $type, $attr) = @_;
    return [$name, {'xsi:type' => 'xsd:uriReference', %$attr}, $value];
  }

The specified methods will work for both autotyping and direct typing, so you
can use either 

  SOAP::Data->type(uriReference => 'http://yahoo.com')>

or just 

  'http://yahoo.com'

and it will be serialized into the same type. For more examples see C<as_*> 
methods in L</"SOAP::Serializer">.

The SOAP::Serializer provides you with C<autotype()>, C<readable()>, C<namespace()>,
C<encodingspace()>, C<encoding()>, C<typelookup()>, C<uri()>, C<multirefinplace()> and 
C<envelope()> methods. All methods (except C<envelope()>) are described in the
L</"SOAP::Lite"> section.

=over 4

=item envelope()

This method allows you to build three kind of envelopes depending on the first 
parameter:

=over 4

=item method

  envelope(method => 'methodname', @parameters);

or

  method('methodname', @parameters);

Lets you build a request/response envelope.

=item fault

  envelope(fault => 'faultcode', 'faultstring', $details);

or 

  fault('faultcode', 'faultstring', $details);

Lets you build a fault envelope. Faultcode will be properly qualified and
details could be string or object.

=item freeform

  envelope(freeform => 'something that I want to serialize');

or

  freeform('something that I want to serialize');

Reserved for nonRPC calls. Lets you build your own payload inside a SOAP 
envelope. All SOAP 1.1 specification rules are enforced, except method 
specific ones. See UDDI::Lite as example.

=item register_ns

The register_ns subroutine allows users to register a global namespace
with the SOAP Envelope. The first parameter is the namespace, the second
parameter to this subroutine is an optional prefix. If a prefix is not
provided, one will be generated automatically for you. All namespaces
registered with the serializer get declared in the <soap:Envelope />
element.

=item find_prefix

The find_prefix subroutine takes a namespace as a parameter and returns
the assigned prefix to that namespace. This eliminates the need to declare
and redeclare namespaces within an envelope. This subroutine is especially
helpful in determining the proper prefix when assigning a type to a
SOAP::Data element. A good example of how this might be used is as follows:

SOAP::Data->name("foo" => $inputParams{'foo'})
	  ->type($client->serializer->find_prefix('urn:Foo').':Foo');

=item xmlschema

The xmlschema subroutine tells SOAP::Lite what XML Schema to use when
serializing XML element values. There are two supported schemas of 
SOAP::Lite, they are:

  http://www.w3.org/1999/XMLSchema, and
  http://www.w3.org/2001/XMLSchema (default)

=back

=back 

For more examples see tests and SOAP::Transport::HTTP.pm

=head2 SOAP::SOM

All calls you are making through object oriented interface will 
return SOAP::SOM object, and you can access actual values with it.
Next example gives you brief overview of the class:

  my $soap = SOAP::Lite .....;
  my $som = $soap->method(@parameters);

  if ($som->fault) { # will be defined if Fault element is in the message
    print $som->faultdetail; # returns value of 'detail' element as
                             # string or object
    $som->faultcode;   #
    $som->faultstring; # also available
    $som->faultactor;  # 
  } else {
    $som->result; # gives you access to result of call  
                  # it could be any data structure, for example reference 
                  # to array if server didi something like: return [1,2];

    $som->paramsout; # gives you access to out parameters if any
                     # for example, you'll get array (1,2) if
                     # server returns ([1,2], 1, 2); 
                     # [1,2] will be returned as $som->result
                     # and $som->paramsall will return ([1,2], 1, 2)
                     # see section IN/OUT, OUT PARAMETERS AND AUTOBINDING
                     # for more information

    $som->paramsall; # gives access to result AND out parameters (if any)
                     # and returns them as one array

    $som->valueof('//myelement'); # returns value(s) (as perl data) of
                                  # 'myelement' if any. All elements in array
                                  # context and only first one in scalar

    $h = $som->headerof('//myheader'); # returns element as SOAP::Header, so
                                       # you can access attributes and values
                                       # with $h->mustUnderstand, $h->actor
                                       # or $h->attr (for all attributes)
  }

SOAP::SOM object gives you access to the deserialized envelope via several 
methods. All methods accept a node path (similar to XPath notations). 
SOM interprets '/' as the root node, '//' as relative location path
('//Body' will find all bodies in document, as well as 
'/Envelope//nums' will find all 'nums' nodes under Envelope node),
'[num]' as node number and '[op num]' with C<op> being a comparison 
operator ('<', '>', '<=', '>=', '!', '=').

All nodes in nodeset will be returned in document order.

=over 4

=item match()

Accepts a path to a node and returns true/false in a boolean context and
a SOM object otherwise. C<valueof()> and C<dataof()> can be used to get 
value(s) of matched node(s).

=item valueof()

Returns the value of a (previously) matched node. It accepts a node path. 
In this case, it returns the value of matched node, but does not change the current
node. Suitable when you want to match a  node and then navigate through
node children:

  $som->match('/Envelope/Body/[1]'); # match method
  $som->valueof('[1]');              # result
  $som->valueof('[2]');              # first out parameter (if present)

The returned value depends on the context. In a scalar context it will return 
the first element from matched nodeset. In an array context it will return 
all matched elements.

=item dataof()        

Same as C<valueof()>, but it returns a L</"SOAP::Data"> object, so you can get 
access to the name, the type and attributes of an element.

=item headerof()

Same as C<dataof()>, but it returns L</"SOAP::Header"> object, so you can get 
access to the name, the type and attributes of an element. Can be used for 
modifying headers (if you want to see updated header inside Header element, 
it's better to use this method instead of C<dataof()> method).

=item namespaceuriof()

Returns the uri associated with the matched element. This uri can also be 
inherited, for example, if you have 

  <a xmlns='http://my.namespace'>
    <b>
       value
    </b>
  </a>

this method will return same value for 'b' element as for 'a'.

=back

SOAP::SOM also provides  methods for direct access to the envelope, the body, 
methods and parameters (both in and out). All these methods return real
values (in most cases it will be a hash reference), if called as object
method. Returned values also depend on context: in an array context it will 
return an array of values and in scalar context it will return the first
element. So, if you want to access the first output parameter, you can call
C<< $param = $som->paramsout >>; 
and you will get it regardless of the actual number of output parameters. 
If you call it as class function (for example, SOAP::SOM::method)
it returns an XPath string that matches the current element 
('/Envelope/Body/[1]' in case of 'method'). The method will return C<undef> 
if not present OR if you try to access an undefined element. To distinguish 
between these two cases you can first access the C<match()> method that 
will return true/false in a boolean context and then get the real value:

  if ($som->match('//myparameter')) {
    $value = $som->valueof; # can be undef too
  } else {
    # doesn't exist
  }

=over 4

=item root()

Returns the value (as hash) of the root element. Do exactly the same as 
C<< $som->valueof('/') >> does.

=item envelope()

Returns the value (as hash) of the C<Envelope> element. Keys in this hash will be 
'Header' (if present), 'Body' and any other (optional) elements. Values will 
be the deserialized header, body, and elements, respectively.
If called as function (C<SOAP::SOM::envelope>) it will return a Xpath string 
that matches the envelope content. Useful when you want just match it and 
then iterate over the content by yourself. Example:

  if ($som->match(SOAP::SOM::envelope)) {
    $som->valueof('Header'); # should give access to header if present
    $som->valueof('Body');   # should give access to body
  } else {
    # hm, are we doing SOAP or what?
  }

=item header()

Returns the value (as hash) of the C<Header> element. If you want to access all 
attributes in the header use:

  # get element as SOAP::Data object 
  $transaction = $som->match(join '/', SOAP::SOM::header, 'transaction')->dataof;
  # then you can access all attributes of 'transaction' element
  $transaction->attr; 

=item headers()

Returns a node set of values with deserialized headers. The difference between 
the C<header()> and C<headers()> methods is that the first gives you access 
to the whole header and second to the headers inside the 'Header' tag:

  $som->headerof(join '/', SOAP::SOM::header, '[1]');
  # gives you first header as SOAP::Header object

  ($som->headers)[0];
  # gives you value of the first header, same as
  $som->valueof(join '/', SOAP::SOM::header, '[1]');

  $som->header->{name_of_your_header_here}
  # gives you value of name_of_your_header_here

=item body()

Returns the value (as hash) of the C<Body> element. 

=item fault()

Returns the value (as hash) of C<Fault> element: C<faultcode>, C<faultstring> and
C<detail>. If C<Fault> element is present, C<result()>, C<paramsin()>, 
C<paramsout()> and C<method()> will return an undef.

=item faultcode()

Returns the value of the C<faultcode> element if present and undef otherwise.

=item faultstring()

Returns the value of the C<faultstring> element if present and undef otherwise.

=item faultactor()

Returns the value of the C<faultactor> element if present and undef otherwise.

=item faultdetail()

Returns the value of the C<detail> element if present and undef otherwise.

=item method()

Returns the value of the method element (all input parameters if you call it on 
a deserialized request envelope, and result/output parameters if you call it
on a deserialized response envelope). Returns undef if the 'Fault' element is 
present.

=item result()

Returns the value of the C<result> of the method call. In fact, it will return 
the first child element (in document order) of the method element.

=item paramsin()

Returns the value(s) of all passed parameters.

=item paramsout()

Returns value(s) of the output parameters. 

=item paramsall()

Returns value(s) of the result AND output parameters as one array.

=item parts()

Return an array of MIME::Entities if the current payload contains attachments, or returns undefined if payload is not MIME multipart.

=item is_multipart()

Returns true if payload is MIME multipart, false otherwise.

=back

=head2 SOAP::Schema

SOAP::Schema gives you ability to load schemas and create stubs according 
to these schemas. Different syntaxes are provided:

=over 4

=item *

  use SOAP::Lite
    service => 'http://www.xmethods.net/sd/StockQuoteService.wsdl',
    # service => 'file:/your/local/path/StockQuoteService.wsdl',
    # service => 'file:./StockQuoteService.wsdl',
  ;
  print getQuote('MSFT'), "\n";

=item *

  use SOAP::Lite;
  print SOAP::Lite
    -> service('http://www.xmethods.net/sd/StockQuoteService.wsdl')
    -> getQuote('MSFT'), "\n";

=item *

  use SOAP::Lite;
  my $service = SOAP::Lite
    -> service('http://www.xmethods.net/sd/StockQuoteService.wsdl');
  print $service->getQuote('MSFT'), "\n";

=back

You can create stub with B<stubmaker> script:

  perl stubmaker.pl http://www.xmethods.net/sd/StockQuoteService.wsdl

and you'll be able to access SOAP services in one line:

  perl "-MStockQuoteService qw(:all)" -le "print getQuote('MSFT')" 

or dynamically:

  perl "-MSOAP::Lite service=>'file:./quote.wsdl'" -le "print getQuote('MSFT')"

Other supported syntaxes with stub(s) are:

=over 4

=item *

  use StockQuoteService ':all';
  print getQuote('MSFT'), "\n";

=item *

  use StockQuoteService;
  print StockQuoteService->getQuote('MSFT'), "\n";

=item *

  use StockQuoteService;
  my $service = StockQuoteService->new;
  print $service->getQuote('MSFT'), "\n";

=back

Support for schemas is limited for now. Though module was tested with dozen
different schemas it won't understand complex objects and will work only
with WSDL. 

=head2 SOAP::Trace

SOAP::Trace provides you with a trace/debug facility for the SOAP::Lite 
library. To activate it you need to specify a list of traceable 
events/parts of SOAP::Lite:

  use SOAP::Lite +trace =>
    [qw(list of available traces here)];

Available events are:

 transport  -- (client) access to request/response for transport layer
 dispatch   -- (server) shows full name of dispatched call 
 result     -- (server) result of method call
 parameters -- (server) parameters for method call
 headers    -- (server) headers of received message
 objects    -- (both)   new/DESTROY calls
 method     -- (both)   parameters for '->envelope(method =>' call
 fault      -- (both)   parameters for '->envelope(fault =>' call
 freeform   -- (both)   parameters for '->envelope(freeform =>' call
 trace      -- (both)   trace enters into some important functions
 debug      -- (both)   details about transport 

For example:

  use SOAP::Lite +trace =>
    [qw(method fault)];

lets you output the parameter values for all your fault/normal envelopes onto STDERR. 
If you want to log it you can either redirect STDERR to some file

  BEGIN { open(STDERR, '>>....'); }

or (preferably) define your own function for a particular event:

  use SOAP::Lite +trace =>
    [ method => sub {'log messages here'}, fault => \&log_faults ];

You can share the same function for several events:

  use SOAP::Lite +trace =>
    [method, fault => \&log_methods_and_faults];

Also you can use 'all' to get all available tracing and use '-' in front of an event to disable particular event:

  use SOAP::Lite +trace =>
    [ all, -transport ]; # to get all logging without transport messages

Finally,

  use SOAP::Lite +trace;

will switch all debugging on.

You can use 'debug' instead of 'trace'. I prefer 'trace', others 'debug'. 
Also C<on_debug> is available for backward compatibility, as in

  use SOAP::Lite;

  my $s = SOAP::Lite
    -> uri('http://tempuri.org/')
    -> proxy('http://beta.search.microsoft.com/search/MSComSearchService.asmx')
    -> on_debug(sub{print@_}) # show you request/response with headers
  ;
  print $s->GetVocabulary(SOAP::Data->name(Query => 'something')->uri('http://tempuri.org/'))
          ->valueof('//FOUND');

or switch it on individually, with

  use SOAP::Lite +trace => debug;

or

  use SOAP::Lite +trace => [debug => sub {'do_what_I_want_here'}];

Compare this with:

  use SOAP::Lite +trace => transport;

which gives you access to B<actual> request/response objects, so you can even 
set/read cookies or do whatever you want there.

The difference between C<debug> and C<transport> is that C<transport> will get 
a HTTP::Request/HTTP::Response object and C<debug> will get a stringified request 
(NOT OBJECT!). It can also be called in other places too. 

=head2 SOAP::Transport

Supports the SOAP Transport architecture. All transports must extend this
class.

=head2 SOAP::Fault

This class gives you access to Fault generated on server side. To make a
Fault message you might simply die on server side and SOAP processor will 
wrap you message as faultstring element and will transfer Fault on client
side. But in some cases you need to have more control over this process and
SOAP::Fault class gives it to you. To use it, simply die with SOAP::Fault
object as a parameter:

  die SOAP::Fault->faultcode('Server.Custom') # will be qualified
                 ->faultstring('Died in server method')
                 ->faultdetail(bless {code => 1} => 'BadError')
                 ->faultactor('http://www.soaplite.com/custom');

faultdetail() and faultactor() methods are optional and since faultcode and
faultstring are required to represent fault message SOAP::Lite will use
default values ('Server' and 'Application error') if not specified.

=head2 SOAP::Utils

This class gives you access to a number of subroutines to assist in data
formating, encoding, etc. Many of the subroutines are private, and are not
documented here, but a few are made public. They are:

=over 4

=item format_datetime

  Returns a valid xsd:datetime string given a time object returned by
  Perl's localtime function. Usage:

  print SOAP::Utils::format_datetime(localtime);

=back

=head2 SOAP::Constants

This class gives you access to number of options that may affect behavior of
SOAP::Lite objects. They are not true contstants, aren't they?

=over

=item $PATCH_HTTP_KEEPALIVE

SOAP::Lite's HTTP Transport module attempts to provide a simple patch to
LWP::Protocol to enable HTTP Keep Alive. By default, this patch is turned
off, if however you would like to turn on the experimental patch change the
constant like so:

  $SOAP::Constants::PATCH_HTTP_KEEPALIVE = 1;

=item $DO_NOT_USE_XML_PARSER

By default SOAP::Lite tries to load XML::Parser and if it fails, then to 
load XML::Parser::Lite. You may skip the first step and use XML::Parser::Lite
even if XML::Parser is presented in your system if assign true value like this:

  $SOAP::Constants::DO_NOT_USE_XML_PARSER = 1;

=item $DO_NOT_USE_CHARSET

By default SOAP::Lite specifies charset in content-type. Since not every
toolkit likes it you have an option to switch it off if you set 
C<$DO_NOT_USE_CHARSET> to true.

=item $DO_NOT_CHECK_CONTENT_TYPE

By default SOAP::Lite verifies that content-type in successful response has
value 'multipart/related' or 'multipart/form-data' for MIME-encoded messages
and 'text/xml' for all other ocassions. SOAP::Lite will raise exception for
all other values. C<$DO_NOT_CHECK_CONTENT_TYPE> when set to true will allow 
you to accept those values as valid.

=back

=head1 FEATURES AND OPTIONS

=head2 DEFAULT SETTINGS

Though this feature looks similar to L<autodispatch|/"AUTODISPATCHING AND SOAP:: PREFIX"> they have (almost) 
nothing in common. It lets you create default object and all objects 
created after that will be cloned from default object and hence get its 
properties. If you want to provide common proxy() or uri() settings for 
all SOAP::Lite objects in your application you may do:

  use SOAP::Lite
    proxy => 'http://localhost/cgi-bin/soap.cgi',
    uri => 'http://my.own.com/My/Examples'
  ;

  my $soap1 = new SOAP::Lite; # will get the same proxy()/uri() as above
  print $soap1->getStateName(1)->result;

  my $soap2 = SOAP::Lite->new; # same thing as above
  print $soap2->getStateName(2)->result;

  # or you may override any settings you want
  my $soap3 = SOAP::Lite->proxy('http://localhost/'); 
  print $soap3->getStateName(1)->result;

B<Any> SOAP::Lite properties can be propagated this way. Changes in object
copies will not affect global settings and you may still change global
settings with C<< SOAP::Lite->self >> call which returns reference to
global object. Provided parameter will update this object and you can
even set it to C<undef>:

  SOAP::Lite->self(undef);

The C<use SOAP::Lite> syntax also lets you specify default event handlers 
for your code. If you have different SOAP objects and want to share the 
same C<on_action()> (or C<on_fault()> for that matter) handler. You can 
specify C<on_action()> during initialization for every object, but 
you may also do:

  use SOAP::Lite 
    on_action => sub {sprintf '%s#%s', @_}
  ;

and this handler will be the default handler for all your SOAP objects. 
You can override it if you specify a handler for a particular object.
See F<t/*.t> for example of on_fault() handler.

Be warned, that since C<use ...> is executed at compile time B<all> C<use> 
statements will be executed B<before> script execution that can make 
unexpected results. Consider code:

  use SOAP::Lite proxy => 'http://localhost/';

  print SOAP::Lite->getStateName(1)->result;

  use SOAP::Lite proxy => 'http://localhost/cgi-bin/soap.cgi';

  print SOAP::Lite->getStateName(1)->result;

B<BOTH> SOAP calls will go to C<'http://localhost/cgi-bin/soap.cgi'>. If
you want to execute C<use> at run-time, put it in C<eval>:

  eval "use SOAP::Lite proxy => 'http://localhost/cgi-bin/soap.cgi'; 1" or die;

or use

  SOAP::Lite->self->proxy('http://localhost/cgi-bin/soap.cgi');

=head2 IN/OUT, OUT PARAMETERS AND AUTOBINDING

SOAP::Lite gives you access to all parameters (both in/out and out) and
also does some additional work for you. Lets consider following example:

  <mehodResponse>
    <res1>name1</res1>
    <res2>name2</res2>
    <res3>name3</res3>
  </mehodResponse>

In that case:

  $result = $r->result; # gives you 'name1'
  $paramout1 = $r->paramsout;      # gives you 'name2', because of scalar context
  $paramout1 = ($r->paramsout)[0]; # gives you 'name2' also
  $paramout2 = ($r->paramsout)[1]; # gives you 'name3'

or

  @paramsout = $r->paramsout; # gives you ARRAY of out parameters
  $paramout1 = $paramsout[0]; # gives you 'res2', same as ($r->paramsout)[0]
  $paramout2 = $paramsout[1]; # gives you 'res3', same as ($r->paramsout)[1]

Generally, if server returns C<return (1,2,3)> you will get C<1> as the result 
and C<2> and C<3> as out parameters.

If the server returns C<return [1,2,3]> you will get an ARRAY from C<result()> and 
C<undef> from C<paramsout()> .
Results can be arbitrary complex: they can be an array of something, they can
be objects, they can be anything and still be returned by C<result()> . If only
one parameter is returned, C<paramsout()> will return C<undef>.

But there is more.
If you have in your output parameters a parameter with the same
signature (name+type) as in the input parameters this parameter will be mapped
into your input automatically. Example:

B<server>:

  sub mymethod {
    shift; # object/class reference
    my $param1 = shift;
    my $param2 = SOAP::Data->name('myparam' => shift() * 2);
    return $param1, $param2;
  }

B<client>:

  $a = 10;
  $b = SOAP::Data->name('myparam' => 12);
  $result = $soap->mymethod($a, $b);

After that, C<< $result == 10 and $b->value == 24 >>! Magic? Sort of. 
Autobinding gives it to you. That will work with objects also with 
one difference: you do not need to worry about the name and the type of
object parameter. Consider the C<PingPong> example (F<examples/My/PingPong.pm> and
F<examples/pingpong.pl>):

B<server>:

  package My::PingPong;

  sub new { 
    my $self = shift;
    my $class = ref($self) || $self;
    bless {_num=>shift} => $class;
  }

  sub next {
    my $self = shift;
    $self->{_num}++;
  }

B<client>:

  use SOAP::Lite +autodispatch =>
    uri => 'urn:', 
    proxy => 'http://localhost/'
  ;

  my $p = My::PingPong->new(10); # $p->{_num} is 10 now, real object returned 
  print $p->next, "\n";          # $p->{_num} is 11 now!, object autobinded

=head2 AUTODISPATCHING AND SOAP:: PREFIX

B<WARNING>: C<autodispatch> feature can have side effects for your application 
and can affect functionality of other modules/libraries because of overloading
UNIVERSAL::AUTOLOAD. All unresolved calls will be dispatched as SOAP calls,
however it could be not what you want in some cases. If so, consider using 
object interface (see C<implementation of OO interface>). 

SOAP::Lite provides an autodispatching feature that lets you create 
code which looks the same for local and remote access.

For example:

  use SOAP::Lite +autodispatch =>
    uri => 'urn:/My/Examples', 
    proxy => 'http://localhost/'
  ;

tells SOAP to 'autodispatch' all calls to the 'http://localhost/' endpoint with
the 'urn:/My/Examples' uri. All consequent method calls can look like:

  print getStateName(1), "\n";
  print getStateNames(12,24,26,13), "\n";
  print getStateList([11,12,13,42])->[0], "\n";
  print getStateStruct({item1 => 10, item2 => 4})->{item2}, "\n";

As you can see, there is no SOAP specific coding at all.

The same logic will work for objects as well:

  print "Session iterator\n";
  my $p = My::SessionIterator->new(10);     
  print $p->next, "\n";  
  print $p->next, "\n";   

This will access the remote My::SessionIterator module, gets an object, and then 
calls remote methods again. The object will be transferred to the server, the 
method is executed there and the result (and the modified object!) will be 
transferred back to the client.

Autodispatch will work B<only> if you do not have the same method in your
code. For example, if you have C<use My::SessionIterator> somewhere in your
code of our previous example, all methods will be resolved locally  and no
SOAP calls will be done. If you want to get access to remote objects/methods 
even in that case, use C<SOAP::> prefix to your methods, like:

  print $p->SOAP::next, "\n";  

See C<pingpong.pl> for example of a script, that works with the same object
locally and remotely.

C<SOAP::> prefix also gives you ability to access methods that have the same
name as methods of SOAP::Lite itself. For example, you want to call method
new() for your class C<My::PingPong> through OO interface. 
First attempt could be:

  my $s = SOAP::Lite 
    -> uri('http://www.soaplite.com/My/PingPong')
    -> proxy('http://localhost/cgi-bin/soap.cgi')
  ;
  my $obj = $s->new(10);

but it won't work, because SOAP::Lite has method new() itself. To provide 
a hint, you should use C<SOAP::> prefix and call will be dispatched remotely:

  my $obj = $s->SOAP::new(10);

You can mix autodispatch and usual SOAP calls in the same code if
you need it. Keep in mind, that calls with SOAP:: prefix should always be a
method call, so if you want to call functions, use C<< SOAP->myfunction() >>
instead of C<SOAP::myfunction()>.

Be warned though Perl has very flexible syntax some versions will complain

  Bareword "autodispatch" not allowed while "strict subs" in use ...

if you try to put 'autodispatch' and '=>' on separate lines. So, keep them
on the same line, or put 'autodispatch' in quotes: 

  use SOAP::Lite 'autodispatch' # DON'T use plus in this case
    => .... 
  ; 

=head2 ACCESSING HEADERS AND ENVELOPE ON SERVER SIDE

SOAP::Lite gives you direct access to all headers and the whole envelope on 
the server side. Consider the following code from My::Parameters.pm:

  sub byname { 
    my($a, $b, $c) = @{pop->method}{qw(a b c)};
    return "a=$a, b=$b, c=$c";
  }

You will get this functionality ONLY if you inherit your class from 
the SOAP::Server::Parameters class. This should keep existing code working and
provides this feature only when you need it.

Every method on server side will be called as class/object method, so it will
get an B<object reference> or a B<class name> as the first parameter, then the 
method parameters, and then an envelope as SOAP::SOM object. Shortly:

  $self [, @parameters] , $envelope

If you have a fixed number of parameters, you can do:

  my $self = shift;
  my($param1, $param2) = @_;

and ignore the envelope. If you need access to the envelope you can do:

  my $envelope = pop; 

since the envelope is always the last element in the parameters list.
The C<byname()> method C<< pop->method >> will return a hash with
parameter names as hash keys and parameter values as hash values:

  my($a, $b, $c) = @{pop->method}{qw(a b c)};

gives you by-name access to your parameters.

=head2 SERVICE DEPLOYMENT. STATIC AND DYNAMIC

Let us scrutinize the deployment process. When designing your SOAP server you 
can consider two kind of deployment: B<static> and B<dynamic>.
For both, static and dynamic,  you should specify C<MODULE>, 
C<MODULE::method>, C<method> or C<PATH/> when creating C<use>ing the 
SOAP::Lite module. The difference between static and dynamic deployment is 
that in case of 'dynamic', any module which is not present will be loaded on
demand. See the L</"SECURITY"> section for detailed description.

Example for B<static> deployment:

  use SOAP::Transport::HTTP;
  use My::Examples;           # module is preloaded 

  SOAP::Transport::HTTP::CGI
    # deployed module should be present here or client will get 'access denied'
    -> dispatch_to('My::Examples') 
    -> handle;

Example for B<dynamic> deployment:

  use SOAP::Transport::HTTP;
  # name is unknown, module will be loaded on demand

  SOAP::Transport::HTTP::CGI
    # deployed module should be present here or client will get 'access denied'
    -> dispatch_to('/Your/Path/To/Deployed/Modules', 'My::Examples') 
    -> handle;

For static deployment you should specify the MODULE name directly. 
For dynamic deployment you can specify the name either directly (in that 
case it will be C<require>d without any restriction) or indirectly, with a PATH
In that case, the ONLY path that will be available will be the PATH given
to the dispatch_to() method). For information how to handle this situation
see L</"SECURITY"> section.

You should also use static binding when you have several different classes 
in one file and want to make them available for SOAP calls.

B<SUMMARY>: 

  dispatch_to(
    # dynamic dispatch that allows access to ALL modules in specified directory
    PATH/TO/MODULES          
    # 1. specifies directory 
    # -- AND --
    # 2. gives access to ALL modules in this directory without limits

    # static dispatch that allows access to ALL methods in particular MODULE
    MODULE 
    #  1. gives access to particular module (all available methods)
    #  PREREQUISITES:
    #    module should be loaded manually (for example with 'use ...')
    #    -- OR --
    #    you can still specify it in PATH/TO/MODULES

    # static dispatch that allows access to particular method ONLY
    MODULE::method 
    # same as MODULE, but gives access to ONLY particular method,
    # so there is not much sense to use both MODULE and MODULE::method 
    # for the same MODULE
  )

In addition to this SOAP::Lite also supports experimental syntax that
allows you bind specific URL or SOAPAction to CLASS/MODULE or object:

  dispatch_with({
    URI => MODULE,        # 'http://www.soaplite.com/' => 'My::Class',
    SOAPAction => MODULE, # 'http://www.soaplite.com/method' => 'Another::Class',
    URI => object,        # 'http://www.soaplite.com/obj' => My::Class->new,
  })

URI is checked before SOAPAction. You may use both C<dispatch_to()> and
C<dispatch_with()> syntax and C<dispatch_with()> has more priority, so
first checked URI, then SOAPAction and only then will be checked 
C<dispatch_to()>. See F<t/03-server.t> for more information and examples.

=head2 SECURITY

Due to security reasons, the current path for perl modules (C<@INC>) will be disabled
once you have chosen dynamic deployment and specified your own C<PATH/>.
If you want to access other modules in your included package you have 
several options:

=over 4

=item 1

Switch to static linking:

   use MODULE;
   $server->dispatch_to('MODULE');

It can be useful also when you want to import something specific
from the deployed modules: 

   use MODULE qw(import_list);

=item 2

Change C<use> to C<require>. The path is unavailable only during 
the initialization part, and it is available again during execution. 
So, if you do C<require> somewhere in your package, it will work.

=item 3

Same thing, but you can do: 

   eval 'use MODULE qw(import_list)'; die if $@;

=item 4

Assign a C<@INC> directory in your package and then make C<use>.
Don't forget to put C<@INC> in C<BEGIN{}> block or it won't work:

   BEGIN { @INC = qw(my_directory); use MODULE }

=back

=head2 COMPRESSION

SOAP::Lite provides you option for enabling compression on wire (for HTTP 
transport only). Both server and client should support this capability, 
but this logic should be absolutely transparent for your application. 

Compression can be enabled by specifying threshold for compression on client 
or server side:

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

For more information see L<COMPRESSION section|SOAP::Transport::HTTP/"COMPRESSION"> 
in HTTP transport documentation.

=head2 OBJECTS-BY-REFERENCE

SOAP::Lite implements an experimental (yet functional) support for
objects-by-reference. You should not see any difference on the client side 
when using this. On the server side you should specify the names of the 
classes you want to be returned by reference (instead of by value) in the 
C<objects_by_reference()> method for your server implementation (see 
soap.pop3, soap.daemon and Apache.pm).

Garbage collection is done on the server side (not earlier than after 600 
seconds of inactivity time), and you can overload the default behavior with 
specific functions for any particular class. 

Binding does not have any special syntax and is implemented on server side 
(see the differences between My::SessionIterator and My::PersistentIterator). 
On the client side, objects will have same type/class as before 
(C<< My::SessionIterator->new() >> will return an object of class 
My::SessionIterator). However, this object is just a stub with an object ID 
inside.

=head2 INTEROPERABILITY

=over 4

=item Microsoft's .NET 

To use .NET client and SOAP::Lite server

=over 4

=item qualify all elements

use fully qualified names for your return values, e.g.: 

  return SOAP::Data->name('myname') 
                   ->type('string')
                   ->uri('http://tempuri.org/')
                   ->value($output);

Use namespace that you specify for URI instead of 'http://tempuri.org/'.

In addition see comment about default encoding in .NET Web Services below.

=back

To use SOAP::Lite client and .NET server

=over 4

=item declare proper soapAction (uri/method) in your call

For example, use C<on_action(sub{join '', @_})>.

=item disable charset in content-type

Specify C<$SOAP::Constants::DO_NOT_USE_CHARSET = 1> somewhere in your code 
after C<use SOAP::Lite> if you are getting error:

  Server found request content type to be 'text/xml; charset=utf-8',
  but expected 'text/xml'

=item qualify all elements

Any of following actions should work:

=over 4

=item use fully qualified name for method parameters

Use C<< SOAP::Data->name(Query  => 'biztalk')->uri('http://tempuri.org/') >> instead of 
C<< SOAP::Data->name('Query'  => 'biztalk') >>.

Example of SOAPsh call (all parameters should be in one line):

  > perl SOAPsh.pl 
    "http://beta.search.microsoft.com/search/mscomsearchservice.asmx" 
    "http://tempuri.org/" 
    "on_action(sub{join '', @_})" 
    "GetVocabulary(SOAP::Data->name(Query  => 'biztalk')->uri('http://tempuri.org/'))"

=item make method in default namespace

instead of 

  my @rc = $soap->call(add => @parms)->result;
  # -- OR --
  my @rc = $soap->add(@parms)->result;

use

  my $method = SOAP::Data->name('add')
                         ->attr({xmlns => 'http://tempuri.org/'});
  my @rc = $soap->call($method => @parms)->result;

=item modify .NET server if you are in charge for that

Stefan Pharies <stefanph@microsoft.com>:

SOAP::Lite uses the SOAP encoding (section 5 of the soap 1.1 spec), and
the default for .NET Web Services is to use a literal encoding. So
elements in the request are unqualified, but your service expects them to 
be qualified. .Net Web Services has a way for you to change the expected 
message format, which should allow you to get your interop working. 
At the top of your class in the asmx, add this attribute (for Beta 1):

  [SoapService(Style=SoapServiceStyle.RPC)]

Another source said it might be this attribute (for Beta 2):

  [SoapRpcService]

Full Web Service text may look like:

  <%@ WebService Language="C#" Class="Test" %>
  using System;
  using System.Web.Services;
  using System.Xml.Serialization;

  [SoapService(Style=SoapServiceStyle.RPC)]
  public class Test : WebService {
    [WebMethod] 
    public int add(int a, int b) {
      return a + b;
    }
  }

Another example from Kirill Gavrylyuk <kirillg@microsoft.com>:

"You can insert [SoapRpcService()] attribute either on your class or on 
operation level".

  <%@ WebService Language=CS class="DataType.StringTest"%>

  namespace DataType {

    using System;
    using System.Web.Services;
    using System.Web.Services.Protocols;
    using System.Web.Services.Description;

   [SoapRpcService()]
   public class StringTest: WebService {
     [WebMethod]
     [SoapRpcMethod()]
     public string RetString(string x) {
       return(x);
     }
   }
 }

Example from Yann Christensen <yannc@microsoft.com>:

  using System;
  using System.Web.Services;
  using System.Web.Services.Protocols;

  namespace Currency {
    [WebService(Namespace="http://www.yourdomain.com/example")]
    [SoapRpcService]
    public class Exchange {
      [WebMethod]
      public double getRate(String country, String country2) {
        return 122.69;
      }
    }
  }

=back

=back

Thanks to 
  Petr Janata <petr.janata@i.cz>, 
  Stefan Pharies <stefanph@microsoft.com>,
  Brian Jepson <bjepson@jepstone.net>, and others 
for description and examples.

=back

=head2 TROUBLESHOOTING

=over 4

=item +autodispatch doesn't work in Perl 5.8

There is a bug in Perl 5.8's UNIVERSAL::AUTOLOAD functionality that prevents
the +autodispatch functionality from working properly. The workaround is to
use dispatch_from instead. Where you might normally do something like this:

   use Some::Module;
   use SOAP::Lite +autodispatch =>
       uri => 'urn:Foo'
       proxy => 'http://...';

You would do something like this:

   use SOAP::Lite dispatch_from(Some::Module) =>
       uri => 'urn:Foo'
       proxy => 'http://...';

=item HTTP transport

See L<TROUBLESHOOTING|SOAP::Transport::HTTP/"TROUBLESHOOTING"> section in 
documentation for HTTP transport.

=item COM interface

=over 4

=item Can't call method "server" on undefined value

Probably you didn't register Lite.dll with 'regsvr32 Lite.dll'

=item Failed to load PerlCtrl runtime

Probably you have two Perl installations in different places and
ActiveState's Perl isn't the first Perl specified in PATH. Rename the
directory with another Perl (at least during the DLL's startup) or put
ActiveState's Perl on the first place in PATH.

=back

=item XML Parsers

=over 4

=item SAX parsers

SAX 2.0 has a known bug in org.xml.sax.helpers.ParserAdapter
     rejects Namespace prefix used before declaration

(http://www.megginson.com/SAX/index.html).

That means that in some cases SOAP messages created by SOAP::Lite may not
be parsed properly by SAX2/Java parser, because Envelope
element contains namespace declarations and attributes that depends on this
declarations. According to XML specification order of these attributes is
not significant. SOAP::Lite does NOT have a problem parsing such messages.

Thanks to Steve Alpert (Steve_Alpert@idx.com) for pointing on it.

=back

=back

=head2 PERFORMANCE

=over 4

=item Processing of XML encoded fragments

SOAP::Lite is based on XML::Parser which is basically wrapper around James 
Clark's expat parser. Expat's behavior for parsing XML encoded string can 
affect processing messages that have lot of encoded entities, like XML 
fragments, encoded as strings. Providing low-level details, parser will call 
char() callback for every portion of processed stream, but individually for 
every processed entity or newline. It can lead to lot of calls and additional
memory manager expenses even for small messages. By contrast, XML messages
which are encoded as base64, don't have this problem and difference in 
processing time can be significant. For XML encoded string that has about 20 
lines and 30 tags, number of call could be about 100 instead of one for
the same string encoded as base64.

Since it is parser's feature there is NO fix for this behavior (let me know
if you find one), especially because you need to parse message you already
got (and you cannot control content of this message), however, if your are
in charge for both ends of processing you can switch encoding to base64 on
sender's side. It will definitely work with SOAP::Lite and it B<may> work with 
other toolkits/implementations also, but obviously I cannot guarantee that.

If you want to encode specific string as base64, just do 
C<< SOAP::Data->type(base64 => $string) >> either on client or on server
side. If you want change behavior for specific instance of SOAP::Lite, you 
may subclass C<SOAP::Serializer>, override C<as_string()> method that is 
responsible for string encoding (take a look into C<as_base64()>) and 
specify B<new> serializer class for your SOAP::Lite object with:

  my $soap = new SOAP::Lite
    serializer => My::Serializer->new,
    ..... other parameters

or on server side:

  my $server = new SOAP::Transport::HTTP::Daemon # or any other server
    serializer => My::Serializer->new,
    ..... other parameters

If you want to change this behavior for B<all> instances of SOAP::Lite, just
substitute C<as_string()> method with C<as_base64()> somewhere in your 
code B<after> C<use SOAP::Lite> and B<before> actual processing/sending:

  *SOAP::Serializer::as_string = \&SOAP::Serializer::as_base64;

Be warned that last two methods will affect B<all> strings and convert them
into base64 encoded. It doesn't make any difference for SOAP::Lite, but it
B<may> make a difference for other toolkits.

=back

=head2 WEBHOSTING INSTALLATION

As soon as you have telnet access to the box and XML::Parser is already
installed there (or you have Perl 5.6 and can use XML::Parser::Lite) you 
may install your own copy of SOAP::Lite even if hosting provider doesn't 
want to do it.

Setup C<PERL5LIB> environment variable. Depending on your shell it may 
look like:

  PERL5LIB=/you/home/directory/lib; export PERL5LIB

C<lib> here is the name of directory where all libraries will be installed 
under your home directory.

Run CPAN module with

  perl -MCPAN -e shell

and run three commands from CPAN shell

  > o conf make_arg -I~/lib
  > o conf make_install_arg -I~/lib
  > o conf makepl_arg LIB=~/lib PREFIX=~ INSTALLMAN1DIR=~/man/man1 INSTALLMAN3DIR=~/man/man3

C<LIB> will specify directory where all libraries will reside. 

C<PREFIX> will specify prefix for all directories (like F<lib>, F<bin>, F<man>, 
though it doesn't work in all cases for some reason).

C<INSTALLMAN1DIR> and C<INSTALLMAN3DIR> specify directories for manuals 
(if you don't specify them, install will fail because it'll try to setup 
it in default directory and you don't have permissions for that).

Then run:

  > install SOAP::Lite

Now in your scripts you need to specify:

  use lib '/your/home/directory/lib';

somewhere before C<'use SOAP::Lite;'>

=head1 BUGS AND LIMITATIONS

=over 4

=item *

No support for multidimensional, partially transmitted and sparse arrays 
(however arrays of arrays are supported, as well as any other data 
structures, and you can add your own implementation with SOAP::Data). 

=item *

Limited support for WSDL schema. 

=item *

XML::Parser::Lite relies on Unicode support in Perl and doesn't do 
entity decoding. 

=item *

Limited support for mustUnderstand and Actor attributes. 

=back

=head1 PLATFORMS

=over 4

=item MacOS

Information about XML::Parser for MacPerl could be found here:
http://bumppo.net/lists/macperl-modules/1999/07/msg00047.html

Compiled XML::Parser for MacOS could be found here:
http://www.perl.com/CPAN-local/authors/id/A/AS/ASANDSTRM/XML-Parser-2.27-bin-1-MacOS.tgz

=back

=head1 AVAILABILITY

You can download the latest version SOAP::Lite for Unix or SOAP::Lite for Win32 from the following sources:

* SOAP::Lite Homepage: http://soaplite.com/
* CPAN:                http://search.cpan.org/search?dist=SOAP-Lite
* Sourceforge:         http://sourceforge.net/projects/soaplite/

You are welcome to send e-mail to the maintainers of SOAP::Lite with your
with your comments, suggestions, bug reports and complaints.

=head1 SEE ALSO

L<SOAP> SOAP/Perl library from Keith Brown ( http://www.develop.com/soap/ ) or
( http://search.cpan.org/search?dist=SOAP )

=head1 ACKNOWLEDGMENTS

A lot of thanks to
  Tony Hong <thong@xmethods.net>,
  Petr Janata <petr.janata@i.cz>,
  Murray Nesbitt <murray@ActiveState.com>,
  Robert Barta <rho@bigpond.net.au>,
  Gisle Aas <gisle@ActiveState.com>,
  Carl K. Cunningham <cc@roberts.de>,
  Graham Glass <graham-glass@mindspring.com>,
  Chris Radcliff <chris@velocigen.com>, 
  Arun Kumar <u_arunkumar@yahoo.com>,
  and many many others 

for providing help, feedback, support, patches and comments. 

=head1 COPYRIGHT

Copyright (C) 2000-2004 Paul Kulchenko. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 AUTHORS

Paul Kulchenko (paulclinger@yahoo.com)
Byrne Reese (byrne@majordojo.com)

=cut
