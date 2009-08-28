use Test;
BEGIN { plan tests => 38 }
use XML::LibXML;
use IO::File;
ok(1);

my $html = "example/test.html";

my $parser = XML::LibXML->new();
{
    my $doc = $parser->parse_html_file($html);
    ok($doc);
}

my $fh = IO::File->new($html) || die "Can't open $html: $!";

my $string;
{
    local $/;
    $string = <$fh>;
}

seek($fh, 0, 0);

ok($string);

$doc = $parser->parse_html_string($string);

ok($doc);

undef $doc;

$doc = $parser->parse_html_fh($fh);

ok($doc);

$fh->close();

# parsing HTML's CGI calling links

my $strhref = <<EOHTML;

<html>
    <body>
        <a href="http:/foo.bar/foobar.pl?foo=bar&bar=foo">
            foo
        </a>
        <p>test
    </body>
</html>
EOHTML

my $htmldoc;

$parser->recover(1);
eval {
    local $SIG{'__WARN__'} = sub { };
    $htmldoc = $parser->parse_html_string( $strhref );
};

# ok( not $@ );
ok( $htmldoc );

print "parse_html_string with encoding...\n";
# encodings
if (eval { require Encode; }) {
  use utf8;

  my $utf_str = "ěščř";

  # w/o 'meta' charset
  $strhref = <<EOHTML;
<html>
  <body>
    <p>$utf_str</p>
  </body>
</html>
EOHTML
   
  ok( Encode::is_utf8($strhref) );
  $htmldoc = $parser->parse_html_string( $strhref );
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  $htmldoc = $parser->parse_html_string( $strhref, 
					 { 
					   encoding => 'UTF-8' 
					 }
					);
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);


  my $iso_str = Encode::encode('iso-8859-2', $strhref);
  $htmldoc = $parser->parse_html_string( $iso_str,
					 {
					   encoding => 'iso-8859-2'
					  }
					);
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  # w/ 'meta' charset
  $strhref = <<EOHTML;
<html>
  <head>
    <meta http-equiv="Content-Type" content="text/html;
      charset=iso-8859-2">
  </head>
  <body>
    <p>$utf_str</p>
  </body>
</html>
EOHTML

  $htmldoc = $parser->parse_html_string( $strhref, { encoding => 'UTF-8' });
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  $iso_str = Encode::encode('iso-8859-2', $strhref);
  $htmldoc = $parser->parse_html_string( $iso_str );
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  $htmldoc = $parser->parse_html_string( $iso_str, { encoding => 'iso-8859-2',
						     URI => 'foo'
						   } );
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);
  ok($htmldoc->URI, 'foo');
} else {
  skip("Encoding related tests require Encode") for 1..14;
}

print "parse example/enc_latin2.html...\n";
# w/ 'meta' charset
{
  use utf8;
  my $utf_str = "ěščř";
  my $test_file = 'example/enc_latin2.html';
  my $fh;

  $htmldoc = $parser->parse_html_file( $test_file );
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);
  
  $htmldoc = $parser->parse_html_file( $test_file, { encoding => 'iso-8859-2',
						     URI => 'foo'
						   });
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);
  ok($htmldoc->URI, 'foo');
  
  open $fh, $test_file;
  $htmldoc = $parser->parse_html_fh( $fh );
  close $fh;
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);
  
  open $fh, $test_file;
  $htmldoc = $parser->parse_html_fh( $fh, { encoding => 'iso-8859-2',
					    URI => 'foo',
					  });
  close $fh;
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->URI, 'foo');
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  if (1000*$] < 5008) {
    skip("skipping for Perl < 5.8") for 1..2;
  } elsif (20627 > XML::LibXML::LIBXML_VERSION) {
    skip("skipping for libxml2 < 2.6.27") for 1..2;
  } else {
    # translate to UTF8 on perl-side
    open $fh, '<:encoding(iso-8859-2)', $test_file;
    $htmldoc = $parser->parse_html_fh( $fh, { encoding => 'UTF-8' });
    close $fh;
    ok( $htmldoc && $htmldoc->getDocumentElement );
    ok($htmldoc->findvalue('//p/text()'), $utf_str);
  }
}

print "parse example/enc2_latin2.html...\n";
# w/o 'meta' charset
{
  use utf8;
  my $utf_str = "ěščř";
  my $test_file = 'example/enc2_latin2.html';
  my $fh;

  $htmldoc = $parser->parse_html_file( $test_file, { encoding => 'iso-8859-2' });
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  open $fh, $test_file;
  $htmldoc = $parser->parse_html_fh( $fh, { encoding => 'iso-8859-2' });
  close $fh;
  ok( $htmldoc && $htmldoc->getDocumentElement );
  ok($htmldoc->findvalue('//p/text()'), $utf_str);

  if (1000*$] < 5008) {
    skip("skipping for Perl < 5.8") for 1..2;
  } else {
    # translate to UTF8 on perl-side
    open $fh, '<:encoding(iso-8859-2)', $test_file;
    $htmldoc = $parser->parse_html_fh( $fh, { encoding => 'UTF-8' } );
    close $fh;
    ok( $htmldoc && $htmldoc->getDocumentElement );
    ok($htmldoc->findvalue('//p/text()'), $utf_str);
  }
}
