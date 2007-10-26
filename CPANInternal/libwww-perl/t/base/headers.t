#!perl -w

use strict;
use Test qw(plan ok);

plan tests => 156;

my($h, $h2);
sub j { join("|", @_) }


require HTTP::Headers;
$h = HTTP::Headers->new;
ok($h);
ok(ref($h), "HTTP::Headers");
ok($h->as_string, "");

$h = HTTP::Headers->new(foo => "bar", foo => "baaaaz", Foo => "baz");
ok($h->as_string, "Foo: bar\nFoo: baaaaz\nFoo: baz\n");

$h = HTTP::Headers->new(foo => ["bar", "baz"]);
ok($h->as_string, "Foo: bar\nFoo: baz\n");

$h = HTTP::Headers->new(foo => 1, bar => 2, foo_bar => 3);
ok($h->as_string, "Bar: 2\nFoo: 1\nFoo-Bar: 3\n");
ok($h->as_string(";"), "Bar: 2;Foo: 1;Foo-Bar: 3;");

ok($h->header("Foo"), 1);
ok($h->header("FOO"), 1);
ok(j($h->header("foo")), 1);
ok($h->header("foo-bar"), 3);
ok($h->header("foo_bar"), 3);
ok($h->header("Not-There"), undef);
ok(j($h->header("Not-There")), "");
ok(eval { $h->header }, undef);
ok($@);

ok($h->header("Foo", 11), 1);
ok($h->header("Foo", [1, 1]), 11);
ok($h->header("Foo"), "1, 1");
ok(j($h->header("Foo")), "1|1");
ok($h->header(foo => 11, Foo => 12, bar => 22), 2);
ok($h->header("Foo"), "11, 12");
ok($h->header("Bar"), 22);
ok($h->header("Bar", undef), 22);
ok(j($h->header("bar", 22)), "");

$h->push_header(Bar => 22);
ok($h->header("Bar"), "22, 22");
$h->push_header(Bar => [23 .. 25]);
ok($h->header("Bar"), "22, 22, 23, 24, 25");
eval { $h->push_header(Bar => 23 .. 25) };
ok($@);
ok(j($h->header("Bar")), "22|22|23|24|25");

$h->clear;
$h->header(Foo => 1);
ok($h->as_string, "Foo: 1\n");
$h->init_header(Foo => 2);
$h->init_header(Bar => 2);
ok($h->as_string, "Bar: 2\nFoo: 1\n");
$h->init_header(Foo => [2, 3]);
$h->init_header(Baz => [2, 3]);
ok($h->as_string, "Bar: 2\nBaz: 2\nBaz: 3\nFoo: 1\n");

eval { $h->init_header(A => 1, B => 2, C => 3) };
ok($@);
ok($h->as_string, "Bar: 2\nBaz: 2\nBaz: 3\nFoo: 1\n");

ok($h->clone->remove_header("Foo"), 1);
ok($h->clone->remove_header("Bar"), 1);
ok($h->clone->remove_header("Baz"), 2);
ok($h->clone->remove_header(qw(Foo Bar Baz Not-There)), 4);
ok($h->clone->remove_header("Not-There"), 0);
ok(j($h->clone->remove_header("Foo")), 1);
ok(j($h->clone->remove_header("Bar")), 2);
ok(j($h->clone->remove_header("Baz")), "2|3");
ok(j($h->clone->remove_header(qw(Foo Bar Baz Not-There))), "1|2|2|3");
ok(j($h->clone->remove_header("Not-There")), "");

$h = HTTP::Headers->new(
    allow => "GET",
    content => "none",
    content_type => "text/html",
    content_md5 => "dummy",
    content_encoding => "gzip",
    content_foo => "bar",
    last_modified => "yesterday",
    expires => "tomorrow",
    etag => "abc",
    date => "today",
    user_agent => "libwww-perl",
    zoo => "foo",
   );
ok($h->as_string, <<EOT);
Date: today
User-Agent: libwww-perl
ETag: abc
Allow: GET
Content-Encoding: gzip
Content-MD5: dummy
Content-Type: text/html
Expires: tomorrow
Last-Modified: yesterday
Content: none
Content-Foo: bar
Zoo: foo
EOT

$h2 = $h->clone;
ok($h->as_string, $h2->as_string);

ok($h->remove_content_headers->as_string, <<EOT);
Allow: GET
Content-Encoding: gzip
Content-MD5: dummy
Content-Type: text/html
Expires: tomorrow
Last-Modified: yesterday
Content-Foo: bar
EOT

ok($h->as_string, <<EOT);
Date: today
User-Agent: libwww-perl
ETag: abc
Content: none
Zoo: foo
EOT

# separate code path for the void context case, so test it as well
$h2->remove_content_headers;
ok($h->as_string, $h2->as_string);

$h->clear;
ok($h->as_string, "");
undef($h2);

$h = HTTP::Headers->new;
ok($h->header_field_names, 0);
ok(j($h->header_field_names), "");

$h = HTTP::Headers->new( etag => 1, foo => [2,3],
			 content_type => "text/plain");
ok($h->header_field_names, 3);
ok(j($h->header_field_names), "ETag|Content-Type|Foo");

{
    my @tmp;
    $h->scan(sub { push(@tmp, @_) });
    ok(j(@tmp), "ETag|1|Content-Type|text/plain|Foo|2|Foo|3");

    @tmp = ();
    eval { $h->scan(sub { push(@tmp, @_); die if $_[0] eq "Content-Type" }) };
    ok($@);
    ok(j(@tmp), "ETag|1|Content-Type|text/plain");

    @tmp = ();
    $h->scan(sub { push(@tmp, @_) });
    ok(j(@tmp), "ETag|1|Content-Type|text/plain|Foo|2|Foo|3");
}

# CONVENIENCE METHODS

$h = HTTP::Headers->new;
ok($h->date, undef);
ok($h->date(time), undef);
ok(j($h->header_field_names), "Date");
ok($h->header("Date") =~ /^[A-Z][a-z][a-z], \d\d .* GMT$/);
{
    my $off = time - $h->date;
    ok($off == 0 || $off == 1); 
}

if ($] < 5.006) {
   Test::skip("Can't call variable method", 1) for 1..13;
}
else {
# other date fields
for my $field (qw(expires if_modified_since if_unmodified_since
		  last_modified))
{
    eval <<'EOT'; die $@ if $@;
    ok($h->$field, undef);
    ok($h->$field(time), undef);
    ok((time - $h->$field) =~ /^[01]$/);
EOT
}
ok(j($h->header_field_names), "Date|If-Modified-Since|If-Unmodified-Since|Expires|Last-Modified");
}

$h->clear;
ok($h->content_type, "");
ok($h->content_type("text/html"), "");
ok($h->content_type, "text/html");
ok($h->content_type("   TEXT  / HTML   ") , "text/html");
ok($h->content_type, "text/html");
ok(j($h->content_type), "text/html");
ok($h->content_type("text/html;\n charSet = \"ISO-8859-1\"; Foo=1 "), "text/html");
ok($h->content_type, "text/html");
ok(j($h->content_type), "text/html|charSet = \"ISO-8859-1\"; Foo=1 ");
ok($h->header("content_type"), "text/html;\n charSet = \"ISO-8859-1\"; Foo=1 ");

ok($h->content_encoding, undef);
ok($h->content_encoding("gzip"), undef);
ok($h->content_encoding, "gzip");
ok(j($h->header_field_names), "Content-Encoding|Content-Type");

ok($h->content_language, undef);
ok($h->content_language("no"), undef);
ok($h->content_language, "no");

ok($h->title, undef);
ok($h->title("This is a test"), undef);
ok($h->title, "This is a test");

ok($h->user_agent, undef);
ok($h->user_agent("Mozilla/1.2"), undef);
ok($h->user_agent, "Mozilla/1.2");

ok($h->server, undef);
ok($h->server("Apache/2.1"), undef);
ok($h->server, "Apache/2.1");

ok($h->from("Gisle\@ActiveState.com"), undef);
ok($h->header("from", "Gisle\@ActiveState.com"));

ok($h->referer("http://www.example.com"), undef);
ok($h->referer, "http://www.example.com");
ok($h->referrer, "http://www.example.com");
ok($h->referer("http://www.example.com/#bar"), "http://www.example.com");
ok($h->referer, "http://www.example.com/");
{
    require URI;
    my $u = URI->new("http://www.example.com#bar");
    $h->referer($u);
    ok($u->as_string, "http://www.example.com#bar");
    ok($h->referer->fragment, undef);
    ok($h->referrer->as_string, "http://www.example.com");
}

ok($h->as_string, <<EOT);
From: Gisle\@ActiveState.com
Referer: http://www.example.com
User-Agent: Mozilla/1.2
Server: Apache/2.1
Content-Encoding: gzip
Content-Language: no
Content-Type: text/html;
 charSet = "ISO-8859-1"; Foo=1
Title: This is a test
EOT

$h->clear;
ok($h->www_authenticate("foo"), undef);
ok($h->www_authenticate("bar"), "foo");
ok($h->www_authenticate, "bar");
ok($h->proxy_authenticate("foo"), undef);
ok($h->proxy_authenticate("bar"), "foo");
ok($h->proxy_authenticate, "bar");

ok($h->authorization_basic, undef);
ok($h->authorization_basic("u"), undef);
ok($h->authorization_basic("u", "p"), "u:");
ok($h->authorization_basic, "u:p");
ok(j($h->authorization_basic), "u|p");
ok($h->authorization, "Basic dTpw");

ok(eval { $h->authorization_basic("u2:p") }, undef);
ok($@);
ok(j($h->authorization_basic), "u|p");

ok($h->proxy_authorization_basic("u2", "p2"), undef);
ok(j($h->proxy_authorization_basic), "u2|p2");
ok($h->proxy_authorization, "Basic dTI6cDI=");

ok($h->as_string, <<EOT);
Authorization: Basic dTpw
Proxy-Authorization: Basic dTI6cDI=
Proxy-Authenticate: bar
WWW-Authenticate: bar
EOT



#---- old tests below -----

$h = new HTTP::Headers
	mime_version  => "1.0",
	content_type  => "text/html";
$h->header(URI => "http://www.oslonett.no/");

ok($h->header("MIME-Version"), "1.0");
ok($h->header('Uri'), "http://www.oslonett.no/");

$h->header("MY-header" => "foo",
	   "Date" => "somedate",
	   "Accept" => ["text/plain", "image/*"],
	  );
$h->push_header("accept" => "audio/basic");

ok($h->header("date"), "somedate");

my @accept = $h->header("accept");
ok(@accept, 3);

$h->remove_header("uri", "date");

my $str = $h->as_string;
my $lines = ($str =~ tr/\n/\n/);
ok($lines, 6);

$h2 = $h->clone;

$h->header("accept", "*/*");
$h->remove_header("my-header");

@accept = $h2->header("accept");
ok(@accept, 3);

@accept = $h->header("accept");
ok(@accept, 1);

# Check order of headers, but first remove this one
$h2->remove_header('mime_version');

# and add this general header
$h2->header(Connection => 'close');

my @x = ();
$h2->scan(sub {push(@x, shift);});
ok(join(";", @x), "Connection;Accept;Accept;Accept;Content-Type;MY-Header");

# Check headers with embedded newlines:
$h = HTTP::Headers->new(
	a => "foo\n\n",
	b => "foo\nbar",
	c => "foo\n\nbar\n\n",
	d => "foo\n\tbar",
	e => "foo\n  bar  ",
	f => "foo\n bar\n  baz\nbaz",
     );
ok($h->as_string("<<\n"), <<EOT);
A: foo<<
B: foo<<
 bar<<
C: foo<<
 bar<<
D: foo<<
\tbar<<
E: foo<<
  bar<<
F: foo<<
 bar<<
  baz<<
 baz<<
EOT


# Check with FALSE $HTML::Headers::TRANSLATE_UNDERSCORE
{
    local($HTTP::Headers::TRANSLATE_UNDERSCORE);
    $HTTP::Headers::TRANSLATE_UNDERSCORE = undef; # avoid -w warning

    $h = HTTP::Headers->new;
    $h->header(abc_abc   => "foo");
    $h->header("abc-abc" => "bar");

    ok($h->header("ABC_ABC"), "foo");
    ok($h->header("ABC-ABC"),"bar");
    ok($h->remove_header("Abc_Abc"));
    ok(!defined($h->header("abc_abc")));
    ok($h->header("ABC-ABC"), "bar");
}

# Check if objects as header values works
require URI;
$h->header(URI => URI->new("http://www.perl.org"));

ok($h->header("URI")->scheme, "http");

$h->clear;
ok($h->as_string, "");

$h->content_type("text/plain");
$h->header(content_md5 => "dummy");
$h->header("Content-Foo" => "foo");
$h->header(Location => "http:", xyzzy => "plugh!");

ok($h->as_string, <<EOT);
Location: http:
Content-MD5: dummy
Content-Type: text/plain
Content-Foo: foo
Xyzzy: plugh!
EOT

my $c = $h->remove_content_headers;
ok($h->as_string, <<EOT);
Location: http:
Xyzzy: plugh!
EOT

ok($c->as_string, <<EOT);
Content-MD5: dummy
Content-Type: text/plain
Content-Foo: foo
EOT

$h = HTTP::Headers->new;
$h->content_type("text/plain");
$h->header(":foo_bar", 1);
$h->push_header(":content_type", "text/html");
ok(j($h->header_field_names), "Content-Type|:content_type|:foo_bar");
ok($h->header('Content-Type'), "text/plain");
ok($h->header(':Content_Type'), undef);
ok($h->header(':content_type'), "text/html");
ok($h->as_string, <<EOT);
Content-Type: text/plain
content_type: text/html
foo_bar: 1
EOT
