use Test;
BEGIN { plan tests => 6 }
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


