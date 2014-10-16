#!/bin/env perl 

BEGIN {
    unless ( grep /blib/, @INC ) {
        chdir 't' if -d 't';
        unshift @INC, '../lib' if -d '../lib';
    }
}

use strict;
use Test;

BEGIN {
    use SOAP::Lite;
    eval { SOAP::Packager::MIME->new->initialize_parser; 1 };
    if ($@) {
        $@ =~ s/ at .+//;
        print "1..0 # Skip: $@";
        exit;
    }
}

BEGIN { plan tests => 18 }

my ( $a, $soap, $d, $s, $r, $serialized, $deserialized );

{    # check attachment deserialization
    $soap = SOAP::Lite->new();
    $soap->init_context();

    ##############################################################################
    print "Attachment deserialization (Content-ID) test(s)...\n";
    $a = $soap->deserializer->deserialize(<<'EOX');
Content-Type: Multipart/Related; boundary=MIME_boundary; type="text/xml"; start="<claim061400a.xml@claiming-it.com>"
SOAPAction: http://schemas.risky-stuff.com/Auto-Claim
Content-Description: This is the optional message description.

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <claim061400a.xml@claiming-it.com>

<?xml version='1.0' ?>
<soap:Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <claim:insurance_claim_auto
          id="insurance_claim_document_id"
          xmlns:claim="http://schemas.risky-stuff.com/Auto-Claim">
      <theSignedForm href="cid:claim061400a.tiff@claiming-it.com" />
      <theCrashPhoto href="cid:claim061400a.jpeg@claiming-it.com" />
      <somexml href="cid:claim061400a.somexml@claiming-it.com" />
      <realxml href="cid:claim061400a.realxml@claiming-it.com" />
      <!-- ... more claim details go here... -->
    </claim:insurance_claim_auto>
  </soap:Body>
</soap:Envelope>

--MIME_boundary
Content-Type: image/tiff
Content-Transfer-Encoding: base64
Content-ID: <claim061400a.tiff@claiming-it.com>

AAECAyAgIAQFBg==
--MIME_boundary
Content-Type: image/jpeg
Content-Transfer-Encoding: binary
Content-ID: <claim061400a.jpeg@claiming-it.com>

...Raw JPEG image..
--MIME_boundary
Content-Type: text/plain
Content-Transfer-Encoding: binary
Content-ID: <claim061400a.somexml@claiming-it.com>

<a><b>c</b></a>
--MIME_boundary
Content-Type: text/xml
Content-Transfer-Encoding: binary
Content-ID: <claim061400a.realxml@claiming-it.com>

<a><b>c</b></a>
--MIME_boundary--

EOX

    ok( ref $a );
    ok(      ref $a
          && ref $a->valueof('//insurance_claim_auto')
          && $a->valueof('//insurance_claim_auto')->{theCrashPhoto} =~ /JPEG/ );
    ok( ref $a && $a->valueof('//theCrashPhoto') =~ /Raw JPEG image/ );
    ok( ref $a && $a->valueof('//theSignedForm') eq "\0\1\2\3   \4\5\6" );
    ok( ref $a && $a->valueof('//somexml') =~ m!<a><b>c</b></a>! );
    ok( ref $a && $a->valueof('//realxml')->{b} eq 'c' );

    ##############################################################################
    print
"Attachment deserialization (Content-ID and Content-Location) test(s)...\n";
    $a = $soap->deserializer->deserialize(<<'EOX');
MIME-Version: 1.0
Content-Type: Multipart/Related; boundary=MIME_boundary; type="text/xml"; start="<http://claiming-it.com/claim061400a.xml>"
Content-Description: This is the optional message description.

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <http://claiming-it.com/claim061400a.xml>
Content-Location: http://claiming-it.com/claim061400a.xml

<?xml version='1.0' ?>
<soap:Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <claim:insurance_claim_auto
          id="insurance_claim_document_id"
          xmlns:claim="http://schemas.risky-stuff.com/Auto-Claim">
      <theSignedForm href="http://claiming-it.com/claim061400a.tiff" />
    </claim:insurance_claim_auto>
  </soap:Body>
</soap:Envelope>

--MIME_boundary
Content-Type: image/tiff
Content-Transfer-Encoding: binary
Content-ID: <http://claiming-it.com/claim061400a.tiff>
Content-Location: http://claiming-it.com/claim061400a.tiff

...binary TIFF image...
--MIME_boundary--

EOX

    ok( ref $a );
    ok(      ref $a
          && ref $a->valueof('//insurance_claim_auto')
          && $a->valueof('//insurance_claim_auto')->{theSignedForm} =~ /TIFF/ );
    ok( ref $a && $a->valueof('//theSignedForm') =~ /binary TIFF image/ );

    ##############################################################################
    print "Attachment deserialization (relative Content-Location) test(s)...\n";

# TODO - this unit test breaks - it does not seem to be picking up a base location
#        from outer Content-Location
    eval {
        $a = $soap->deserializer->deserialize(<<'EOX'); };
MIME-Version: 1.0
Content-Type: Multipart/Related; boundary=MIME_boundary; type="text/xml"; start="<http://claiming-it.com/claim061400a.xml>"
Content-Description: This is the optional message description.
Content-Location: http://claiming-it.com/

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <http://claiming-it.com/claim061400a.xml>
Content-Location: claim061400a.xml

<?xml version='1.0' ?>
<soap:Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <claim:insurance_claim_auto
          id="insurance_claim_document_id"
          xmlns:claim="http://schemas.risky-stuff.com/Auto-Claim">
      <theSignedForm href="cid:claim061400a.tiff" />
    </claim:insurance_claim_auto>
  </soap:Body>
</soap:Envelope>

--MIME_boundary
Content-Type: image/tiff
Content-Transfer-Encoding: binary
Content-Id: <claim061400a.tiff>
Content-Location: claim061400a.tiff

...binary TIFF image...
--MIME_boundary--

EOX
    ok( ref $a );
    ok(      ref $a
          && ref $a->valueof('//insurance_claim_auto')
          && $a->valueof('//insurance_claim_auto')->{theSignedForm} =~ /TIFF/ );
    ok( ref $a && $a->valueof('//theSignedForm') =~ /binary TIFF image/ );

    ##############################################################################
    print
      "Attachment deserialization (no default Content-Location) test(s)...\n";
    $a = $soap->deserializer->deserialize(<<'EOX');
MIME-Version: 1.0
Content-Type: Multipart/Related; boundary=MIME_boundary; type="text/xml"; start="<b6f4ccrt@15.4.9.92/s445>"
Content-Description: This is the optional message description.

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <b6f4ccrt@15.4.9.92/s445>
Content-Location: claim061400a.xml

<?xml version='1.0' ?>
<soap:Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <claim:insurance_claim_auto
          id="insurance_claim_document_id"
          xmlns:claim="http://schemas.risky-stuff.com/Auto-Claim">
      <theSignedForm href="cid:a34ccrt@15.4.9.92/s445" />
    </claim:insurance_claim_auto>
  </soap:Body>
</soap:Envelope>

--MIME_boundary
Content-Type: image/tiff
Content-Transfer-Encoding: binary
Content-ID: <a34ccrt@15.4.9.92/s445>
Content-Location: the_signed_form.tiff

...binary TIFF image...
--MIME_boundary-

EOX

    #      <theSignedForm href="cid:the_signed_form.tiff"/>

    ok( ref $a );
    ok(      ref $a
          && ref $a->valueof('//insurance_claim_auto')
          && $a->valueof('//insurance_claim_auto')->{theSignedForm} =~ /TIFF/ );
    ok( ref $a && $a->valueof('//theSignedForm') =~ /binary TIFF image/ );

    # check attachment of composite element
    $soap = SOAP::Lite->new();
    $soap->init_context();

    ##############################################################################
    print "Attachment deserialization (Nested Attachment) test(s)...\n";
    $a = $soap->deserializer->deserialize(<<'EOX');
Content-Type: Multipart/Related; boundary=MIME_boundary; type="text/xml"; start="<claimstart.xml@claiming-it.com>"
SOAPAction: http://schemas.risky-stuff.com/Auto-Claim
Content-Description: This is the optional message description.

--MIME_boundary
Content-Type: text/xml; charset=UTF-8
Content-Transfer-Encoding: 8bit
Content-ID: <claimstart.xml@claiming-it.com>

<?xml version='1.0' ?>
<soap:Envelope
    xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
  <soap:Body>
    <claim:insurance_claim_auto
          id="insurance_claim_document_id"
          xmlns:claim="http://schemas.risky-stuff.com/Auto-Claim">
      <email href="cid:email.eml@claiming-it.com" />
    </claim:insurance_claim_auto>
  </soap:Body>
</soap:Envelope>

--MIME_boundary
Content-Type: message/rfc822
Content-Transfer-Encoding: binary
Content-ID: <email.eml@claiming-it.com>

MIME-Version: 1.0
X-Mailer: MIME::Lite 3.01 (F2.74; T1.24; A1.74; B3.07; Q3.07)
Date: Thu, 30 Apr 2009 15:59:49 -0400
To: foo@bar.com
Subject: multipart message
Content-Transfer-Encoding: binary
Content-Type: multipart/related; boundary="_----------=_1241121589160890"

This is a multi-part message in MIME format.

--_----------=_1241121589160890
Content-Disposition: inline
Content-Length: 36
Content-Transfer-Encoding: binary
Content-Type: text/html

<body>Here's <i>my</i> text</body> 
--_----------=_1241121589160890
Content-Disposition: inline
Content-Length: 16
Content-Transfer-Encoding: binary
Content-Type: text/plain

 Here's my text 
--_----------=_1241121589160890--

--MIME_boundary--

EOX

    ok( ref $a );
    ok(      ref $a
          && ref $a->valueof('//insurance_claim_auto')
          && $a->valueof('//insurance_claim_auto')->{email} =~
          m{my(</i>)? text} );
    ok( ref $a && $a->valueof('//email') =~ m{my(</i>)? text} );
}
