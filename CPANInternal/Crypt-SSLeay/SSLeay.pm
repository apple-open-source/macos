package Crypt::SSLeay;

use strict;
use vars '$VERSION';
$VERSION = '0.57';

eval {
    require XSLoader;
    XSLoader::load('Crypt::SSLeay', $VERSION);
    1;
}
or do {
    require DynaLoader;
    use vars '@ISA'; # not really locally scoped, it just looks that way
    @ISA = qw(DynaLoader);
    bootstrap Crypt::SSLeay $VERSION;
};

use vars qw(%CIPHERS);
%CIPHERS = (
   'NULL-MD5'     => "No encryption with a MD5 MAC",
   'RC4-MD5'      => "128 bit RC4 encryption with a MD5 MAC",
   'EXP-RC4-MD5'  => "40 bit RC4 encryption with a MD5 MAC",
   'RC2-CBC-MD5'  => "128 bit RC2 encryption with a MD5 MAC",
   'EXP-RC2-CBC-MD5' => "40 bit RC2 encryption with a MD5 MAC",
   'IDEA-CBC-MD5' => "128 bit IDEA encryption with a MD5 MAC",
   'DES-CBC-MD5'  => "56 bit DES encryption with a MD5 MAC",
   'DES-CBC-SHA'  => "56 bit DES encryption with a SHA MAC",
   'DES-CBC3-MD5' => "192 bit EDE3 DES encryption with a MD5 MAC",
   'DES-CBC3-SHA' => "192 bit EDE3 DES encryption with a SHA MAC",
   'DES-CFB-M1'   => "56 bit CFB64 DES encryption with a one byte MD5 MAC",
);

use Crypt::SSLeay::X509;

# A xsupp bug made this nessesary
sub Crypt::SSLeay::CTX::DESTROY  { shift->free; }
sub Crypt::SSLeay::Conn::DESTROY { shift->free; }
sub Crypt::SSLeay::X509::DESTROY { shift->free; }

1;

__END__

=head1 NAME

Crypt::SSLeay - OpenSSL support for LWP

=head1 SYNOPSIS

  lwp-request https://www.example.com

  use LWP::UserAgent;
  my $ua  = LWP::UserAgent->new;
  my $req = HTTP::Request->new('GET', 'https://www.example.com/');
  my $res = $ua->request($req);
  print $res->content, "\n";

=head1 DESCRIPTION

This document describes C<Crypt::SSLeay> version 0.57, released
2007-09-17.

This perl module provides support for the https protocol under LWP,
to allow an C<LWP::UserAgent> object to perform GET, HEAD and POST
requests. Please see LWP for more information on POST requests.

The C<Crypt::SSLeay> package provides C<Net::SSL>, which is loaded
by C<LWP::Protocol::https> for https requests and provides the
necessary SSL glue.

This distribution also makes following deprecated modules available:

  Crypt::SSLeay::CTX
  Crypt::SSLeay::Conn
  Crypt::SSLeay::X509

Work on Crypt::SSLeay has been continued only to provide https
support for the LWP (libwww-perl) libraries.

=head1 ENVIRONMENT VARIABLES

The following environment variables change the way
C<Crypt::SSLeay> and C<Net::SSL> behave.

  # proxy support
  $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';

  # proxy_basic_auth
  $ENV{HTTPS_PROXY_USERNAME} = 'username';
  $ENV{HTTPS_PROXY_PASSWORD} = 'password';  

  # debugging (SSL diagnostics)
  $ENV{HTTPS_DEBUG} = 1;

  # default ssl version
  $ENV{HTTPS_VERSION} = '3';

  # client certificate support
  $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
  $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

  # CA cert peer verification
  $ENV{HTTPS_CA_FILE}   = 'certs/ca-bundle.crt';
  $ENV{HTTPS_CA_DIR}    = 'certs/';

  # Client PKCS12 cert support
  $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
  $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

=head1 INSTALL

=head2 OpenSSL

You must have OpenSSL or SSLeay installed before compiling 
this module. You can get the latest OpenSSL package from:

  http://www.openssl.org/

On Debian systems, you will need to install the libssl-dev package,
at least for the duration of the build (it may be removed afterwards).

Other package-based systems may require something similar. The key
is that Crypt::SSLeay makes calls to the OpenSSL library, and how
to do so is specified in the C header files that come with the
library.  Some systems break out the header files into a separate
package from that of the libraries. Once the program has been built,
you don't need the headers any more.

When installing openssl make sure your config looks like:

  ./config --openssldir=/usr/local/openssl
 or
  ./config --openssldir=/usr/local/ssl

If you are planning on upgrading the default OpenSSL libraries on
a system like RedHat, (not recommended), then try something like:

  ./config --openssldir=/usr --shared

The --shared option to config will set up building the .so 
shared libraries which is important for such systems. This is
followed by:

  make
  make test
  make install

This way Crypt::SSLeay will pick up the includes and 
libraries automatically. If your includes end up
going into a separate directory like /usr/local/include,
then you may need to symlink /usr/local/openssl/include
to /usr/local/include

=head2 Crypt::SSLeay

The latest Crypt::SSLeay can be found at your nearest CPAN,
as well as:

  http://search.cpan.org/dist/Crypt-SSLeay/

Once you have downloaded it, Crypt::SSLeay installs easily 
using the C<make> * commands as shown below.  

  perl Makefile.PL
  make
  make test
  make install

  * use nmake or dmake on Win32

For unattended (batch) installations, to be absolutely certain that
F<Makefile.PL> does not prompt for questions on STDIN, set the
following environment variable beforehand:

  PERL_MM_USE_DEFAULT=1

(This is true for any CPAN module that uses C<ExtUtils::MakeMaker>).

=head3 Windows

C<Crypt::SSLeay> builds correctly with Strawberry Perl.

For Activestate users, the ActiveState company does not have a
permit from the Canadian Federal Government to distribute cryptographic
software. This prevents C<Crypt::SSLeay> from being distributed as
a PPM package from their repository. See
L<http://aspn.activestate.com/ASPN/docs/ActivePerl/5.8/faq/ActivePerl-faq2.html#crypto_packages>
for more information on this issue.

You may download it from Randy Kobes's PPM repository by using
the following command:

  ppm install http://theoryx5.uwinnipeg.ca/ppms/Crypt-SSLeay.ppd

An alternative is to add the uwinnipeg.ca PPM repository to your
local installation. See L<http://cpan.uwinnipeg.ca/htdocs/faqs/ppm.html>
for more details.

=head3 VMS

It is assumed that the OpenSSL installation is located at
C</ssl$root>. Define this logical to point to the appropriate
place in the filesystem.

=head1 PROXY SUPPORT

LWP::UserAgent and Crypt::SSLeay have their own versions of 
proxy support. Please read these sections to see which one
is appropriate.

=head2 LWP::UserAgent proxy support

LWP::UserAgent has its own methods of proxying which may work for
you and is likely to be incompatible with Crypt::SSLeay proxy support.
To use LWP::UserAgent proxy support, try something like:

  my $ua = new LWP::UserAgent;
  $ua->proxy([qw( https http )], "$proxy_ip:$proxy_port");

At the time of this writing, libwww v5.6 seems to proxy https 
requests fine with an Apache mod_proxy server.  It sends a line like:

  GET https://www.example.com HTTP/1.1

to the proxy server, which is not the CONNECT request that
some proxies would expect, so this may not work with other
proxy servers than mod_proxy. The CONNECT method is used
by Crypt::SSLeay's internal proxy support.

=head2 Crypt::SSLeay proxy support

For native Crypt::SSLeay proxy support of https requests,
you need to set the environment variable C<HTTPS_PROXY> to your 
proxy server and port, as in:

  # proxy support
  $ENV{HTTPS_PROXY} = 'http://proxy_hostname_or_ip:port';
  $ENV{HTTPS_PROXY} = '127.0.0.1:8080';

Use of the C<HTTPS_PROXY> environment variable in this way 
is similar to C<LWP::UserAgent->env_proxy()> usage, but calling
that method will likely override or break the Crypt::SSLeay
support, so do not mix the two.

Basic auth credentials to the proxy server can be provided 
this way:

  # proxy_basic_auth
  $ENV{HTTPS_PROXY_USERNAME} = 'username';
  $ENV{HTTPS_PROXY_PASSWORD} = 'password';  

For an example of LWP scripting with C<Crypt::SSLeay> native proxy
support, please look at the F<eg/lwp-ssl-test> script in the 
C<Crypt::SSLeay> distribution.

=head1 CLIENT CERTIFICATE SUPPORT

Client certificates are supported. PEM0encoded certificate and
private key files may be used like this:

  $ENV{HTTPS_CERT_FILE} = 'certs/notacacert.pem';
  $ENV{HTTPS_KEY_FILE}  = 'certs/notacakeynopass.pem';

You may test your files with the F<eg/net-ssl-test> program,
bundled with the distribution, by issuing a command like:

  perl eg/net-ssl-test -cert=certs/notacacert.pem \
    -key=certs/notacakeynopass.pem -d GET $HOST_NAME

Additionally, if you would like to tell the client where
the CA file is, you may set these.

  $ENV{HTTPS_CA_FILE} = "some_file";
  $ENV{HTTPS_CA_DIR}  = "some_dir";

There is no sample CA cert file at this time for testing,
but you may configure F<eg/net-ssl-test> to use your CA cert
with the -CAfile option. (TODO: then what is the ./certs
directory in the distribution?)

=head2 Creating a test certificate

To create simple test certificates with OpenSSL, you may
run the following command:

  openssl req -config /usr/local/openssl/openssl.cnf \
    -new -days 365 -newkey rsa:1024 -x509 \
    -keyout notacakey.pem -out notacacert.pem 

To remove the pass phrase from the key file, run:

  openssl rsa -in notacakey.pem -out notacakeynopass.pem

=head2 PKCS12 support

The directives for enabling use of PKCS12 certificates is:

  $ENV{HTTPS_PKCS12_FILE}     = 'certs/pkcs12.pkcs12';
  $ENV{HTTPS_PKCS12_PASSWORD} = 'PKCS12_PASSWORD';

Use of this type of certificate takes precedence over previous
certificate settings described. (TODO: unclear? Meaning "the
presence of this type of certificate??)

=head1 SSL versions

Crypt::SSLeay tries very hard to connect to I<any> SSL web server
accomodating servers that are buggy, old or simply
not standards-compliant. To this effect, this module will
try SSL connections in this order:

  SSL v23 - should allow v2 and v3 servers to pick their best type
  SSL v3  - best connection type
  SSL v2  - old connection type

Unfortunately, some servers seem not to handle a reconnect
to SSL v3 after a failed connect of SSL v23 is tried,
so you may set before using LWP or Net::SSL:

  $ENV{HTTPS_VERSION} = 3;

to force a version 3 SSL connection first. At this time only a
version 2 SSL connection will be tried after this, as the connection
attempt order remains unchanged by this setting.

=head1 ACKNOWLEDGEMENTS

Many thanks to Gisle Aas for writing this module and many others
including libwww, for perl. The web will never be the same :)

Ben Laurie deserves kudos for his excellent patches for better error
handling, SSL information inspection, and random seeding.

Thanks to Dongqiang Bai for host name resolution fix when using a
proxy.

Thanks to Stuart Horner of Core Communications, Inc. who found the
need for building --shared OpenSSL libraries.

Thanks to Pavel Hlavnicka for a patch for freeing memory when using
a pkcs12 file, and for inspiring more robust read() behavior.

James Woodyatt is a champ for finding a ridiculous memory leak that
has been the bane of many a Crypt::SSLeay user.

Thanks to Bryan Hart for his patch adding proxy support,
and thanks to Tobias Manthey for submitting another approach.

Thanks to Alex Rhomberg for Alpha linux ccc patch.

Thanks to Tobias Manthey for his patches for client certificate
support.

Thanks to Daisuke Kuroda for adding PKCS12 certificate support.

Thanks to Gamid Isayev for CA cert support and insights into error
messaging.

Thanks to Jeff Long for working through a tricky CA cert SSLClientVerify
issue.

Thanks to Chip Turner for patch to build under perl 5.8.0.

Thanks to Joshua Chamas for the time he spent maintaining the
module.

Thanks to Jeff Lavallee for help with alarms on read failures (CPAN
bug #12444).

Thanks to Guenter Knauf for significant improvements in configuring
things in Win32 and Netware lands and Jan Dubois for various
suggestions for improvements.

=head1 SEE ALSO

=over 4

=item Net::SSL

If you have downloaded this distribution as of a dependency
of another distribution, it's probably due to this module
(which is included in this distribution).

=item Net::SSLeay

A module that offers access to the OpenSSL API directly from Perl.

  http://search.cpan.org/dist/Net_SSLeay.pm/

=item http://www.openssl.org/related/binaries.html

Pointers on where to find OpenSSL binary packages (Windows).

=back

=head1 SUPPORT

For use of Crypt::SSLeay & Net::SSL with perl's LWP, please
send email to C<libwww@perl.org>.

For OpenSSL or general SSL support please email the 
openssl user mailing list at C<openssl-users@openssl.org>.
This includes issues associated with building and installing
OpenSSL on one's system.

Please report all bugs at
L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=Crypt-SSLeay>.

This module was originally written by Gisle Aas, and was subsequently
maintained by Joshua Chamas. It is currently maintained by David
Landgren.

=head1 COPYRIGHT

 Copyright (c) 2006-2007 David Landgren.
 Copyright (c) 1999-2003 Joshua Chamas.
 Copyright (c) 1998 Gisle Aas.

This program is free software; you can redistribute 
it and/or modify it under the same terms as Perl itself. 

=cut
