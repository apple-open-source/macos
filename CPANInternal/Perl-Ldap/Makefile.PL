# -*- perl -*-

use inc::Module::Install;

perl_version    5.004;

name		('perl-ldap');
module_name	('Net::LDAP');
author		('Graham Barr <gbarr@cpan.org>');
version_from	('lib/Net/LDAP.pm');
license		('perl');
abstract	('LDAP client library');

sign(1);

check_nmake();      # check and download nmake.exe for Win32

requires        ( 'Convert::ASN1' => 0.07);

include_deps	('Test::More');
include 	('ExtUtils::AutoInstall');

features(
  'SASL authentication' => [
    -default => 0,
    'Authen::SASL'  => '2.00',
    #recommends('Digest::MD5'),		# for DIGEST-MD5 SASL mech
    #recommends('Digest::HMAC_MD5'),	# for CRAM-MD5 SASL mech
    #recommends('GSSAPI'),		# for GSSAPI SASL mech
  ],
  'LDAP URLs' => [
    -default => 0,
    'URI::ldap' => 1.10,
  ],
  'LDAPS' => [
    -default => 0,
    'IO::Socket::SSL' => 0.81,
  ],
  'Read/Write DSML files' => [
    -default => 0,
    'MIME::Base64'  => 0,
    'XML::SAX::Writer' => 0,
    # dependencies of XML::SAX::Writer:
    #'Text::Iconv' => 0,
    #'XML::Filter::BufferText' => 0,
    #'XML::SAX' => 0,
    #'XML::NamespaceSupport' => 0,
    #'File::Temp' => 0
  ],
  'Read/Write LDIF files' => [
    -default => 0,
    'MIME::Base64'  => 0,
  ],
);

auto_install_now();

WriteAll;
