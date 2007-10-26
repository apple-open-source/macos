#!perl -w

use strict;
use Test qw(plan ok skip);

plan tests => 95;

require HTTP::Message;

my($m, $m2, @parts);

$m = HTTP::Message->new;
ok($m);
ok(ref($m), "HTTP::Message");
ok(ref($m->headers), "HTTP::Headers");
ok($m->as_string, "\n");
ok($m->headers->as_string, "");
ok($m->headers_as_string, "");
ok($m->content, "");

$m->header("Foo", 1);
ok($m->as_string, "Foo: 1\n\n");

$m2 = HTTP::Message->new($m->headers);
$m2->header(bar => 2);
ok($m->as_string, "Foo: 1\n\n");
ok($m2->as_string, "Bar: 2\nFoo: 1\n\n");

$m2 = HTTP::Message->new($m->headers, "foo");
ok($m2->as_string, "Foo: 1\n\nfoo\n");
ok($m2->as_string("<<\n"), "Foo: 1<<\n<<\nfoo");
$m2 = HTTP::Message->new($m->headers, "foo\n");
ok($m2->as_string, "Foo: 1\n\nfoo\n");

$m = HTTP::Message->new([a => 1, b => 2], "abc");
ok($m->as_string, "A: 1\nB: 2\n\nabc\n");

$m = HTTP::Message->parse("");
ok($m->as_string, "\n");
$m = HTTP::Message->parse("\n");
ok($m->as_string, "\n");
$m = HTTP::Message->parse("\n\n");
ok($m->as_string, "\n\n");
ok($m->content, "\n");

$m = HTTP::Message->parse("foo");
ok($m->as_string, "\nfoo\n");
$m = HTTP::Message->parse("foo: 1");
ok($m->as_string, "Foo: 1\n\n");
$m = HTTP::Message->parse("foo: 1\n\nfoo");
ok($m->as_string, "Foo: 1\n\nfoo\n");
$m = HTTP::Message->parse(<<EOT);
FOO : 1
 2
  3
   4
bar:
 1
Baz: 1

foobarbaz
EOT
ok($m->as_string, <<EOT);
Bar: 
 1
Baz: 1
Foo: 1
 2
  3
   4

foobarbaz
EOT

$m = HTTP::Message->parse(<<EOT);
Date: Fri, 18 Feb 2005 18:33:46 GMT
Connection: close
Content-Type: text/plain

foo:bar
second line
EOT
ok($m->content(""), <<EOT);
foo:bar
second line
EOT
ok($m->as_string, <<EOT);
Connection: close
Date: Fri, 18 Feb 2005 18:33:46 GMT
Content-Type: text/plain

EOT

$m = HTTP::Message->parse("  abc\nfoo: 1\n");
ok($m->as_string, "\n  abc\nfoo: 1\n");
$m = HTTP::Message->parse(" foo : 1\n");
ok($m->as_string, "\n foo : 1\n");
$m = HTTP::Message->parse("\nfoo: bar\n");
ok($m->as_string, "\nfoo: bar\n");

$m = HTTP::Message->new([a => 1, b => 2], "abc");
ok($m->content("foo\n"), "abc");
ok($m->content, "foo\n");

$m->add_content("bar");
ok($m->content, "foo\nbar");
$m->add_content(\"\n");
ok($m->content, "foo\nbar\n");

ok(ref($m->content_ref), "SCALAR");
ok(${$m->content_ref}, "foo\nbar\n");
${$m->content_ref} =~ s/[ao]/i/g;
ok($m->content, "fii\nbir\n");

$m->clear;
ok($m->headers->header_field_names, 0);
ok($m->content, "");

ok($m->parts, undef);
$m->parts(HTTP::Message->new,
	  HTTP::Message->new([a => 1], "foo"),
	  HTTP::Message->new(undef, "bar\n"),
         );
ok($m->parts->as_string, "\n");

my $str = $m->as_string;
$str =~ s/\r/<CR>/g;
ok($str, <<EOT);
Content-Type: multipart/mixed; boundary=xYzZY

--xYzZY<CR>
<CR>
<CR>
--xYzZY<CR>
A: 1<CR>
<CR>
foo<CR>
--xYzZY<CR>
<CR>
bar
<CR>
--xYzZY--<CR>
EOT

$m2 = HTTP::Message->new;
$m2->parts($m);

$str = $m2->as_string;
$str =~ s/\r/<CR>/g;
ok($str =~ /boundary=(\S+)/);


ok($str, <<EOT);
Content-Type: multipart/mixed; boundary=$1

--$1<CR>
Content-Type: multipart/mixed; boundary=xYzZY<CR>
<CR>
--xYzZY<CR>
<CR>
<CR>
--xYzZY<CR>
A: 1<CR>
<CR>
foo<CR>
--xYzZY<CR>
<CR>
bar
<CR>
--xYzZY--<CR>
<CR>
--$1--<CR>
EOT

@parts = $m2->parts;
ok(@parts, 1);

@parts = $parts[0]->parts;
ok(@parts, 3);
ok($parts[1]->header("A"), 1);

$m2->parts([HTTP::Message->new]);
@parts = $m2->parts;
ok(@parts, 1);

$m2->parts([]);
@parts = $m2->parts;
ok(@parts, 0);

$m->clear;
$m2->clear;

$m = HTTP::Message->new([content_type => "message/http; boundary=aaa",
                        ],
                        <<EOT);
GET / HTTP/1.1
Host: www.example.com:8008

EOT

@parts = $m->parts;
ok(@parts, 1);
$m2 = $parts[0];
ok(ref($m2), "HTTP::Request");
ok($m2->method, "GET");
ok($m2->uri, "/");
ok($m2->protocol, "HTTP/1.1");
ok($m2->header("Host"), "www.example.com:8008");
ok($m2->content, "");

$m->content(<<EOT);
HTTP/1.0 200 OK
Content-Type: text/plain

Hello
EOT

$m2 = $m->parts;
ok(ref($m2), "HTTP::Response");
ok($m2->protocol, "HTTP/1.0");
ok($m2->code, "200");
ok($m2->message, "OK");
ok($m2->content_type, "text/plain");
ok($m2->content, "Hello\n");

eval { $m->parts(HTTP::Message->new, HTTP::Message->new) };
ok($@);

$m->add_part(HTTP::Message->new([a=>[1..3]], "a"));
$str = $m->as_string;
$str =~ s/\r/<CR>/g;
ok($str, <<EOT);
Content-Type: multipart/mixed; boundary=xYzZY

--xYzZY<CR>
Content-Type: message/http; boundary=aaa<CR>
<CR>
HTTP/1.0 200 OK
Content-Type: text/plain

Hello
<CR>
--xYzZY<CR>
A: 1<CR>
A: 2<CR>
A: 3<CR>
<CR>
a<CR>
--xYzZY--<CR>
EOT

$m->add_part(HTTP::Message->new([b=>[1..3]], "b"));

$str = $m->as_string;
$str =~ s/\r/<CR>/g;
ok($str, <<EOT);
Content-Type: multipart/mixed; boundary=xYzZY

--xYzZY<CR>
Content-Type: message/http; boundary=aaa<CR>
<CR>
HTTP/1.0 200 OK
Content-Type: text/plain

Hello
<CR>
--xYzZY<CR>
A: 1<CR>
A: 2<CR>
A: 3<CR>
<CR>
a<CR>
--xYzZY<CR>
B: 1<CR>
B: 2<CR>
B: 3<CR>
<CR>
b<CR>
--xYzZY--<CR>
EOT

$m = HTTP::Message->new;
$m->content_ref(\my $foo);
ok($m->content_ref, \$foo);
$foo = "foo";
ok($m->content, "foo");
$m->add_content("bar");
ok($foo, "foobar");
ok($m->as_string, "\nfoobar\n");
$m->content_type("message/foo");
$m->parts(HTTP::Message->new(["h", "v"], "C"));
ok($foo, "H: v\r\n\r\nC");
$foo =~ s/C/c/;
$m2 = $m->parts;
ok($m2->content, "c");

$m = HTTP::Message->new;
$foo = [];
$m->content($foo);
ok($m->content, $foo);
ok(${$m->content_ref}, $foo);
ok(${$m->content_ref([])}, $foo);
ok($m->content_ref != $foo);
eval {$m->add_content("x")};
ok($@ && $@ =~ /^Can't append to ARRAY content/);

$foo = sub { "foo" };
$m->content($foo);
ok($m->content, $foo);
ok(${$m->content_ref}, $foo);

$m->content_ref($foo);
ok($m->content, $foo);
ok($m->content_ref, $foo);

eval {$m->content_ref("foo")};
ok($@ && $@ =~ /^Setting content_ref to a non-ref/);

$m->content_ref(\"foo");
eval {$m->content("bar")};
ok($@ && $@ =~ /^Modification of a read-only value/);

$foo = "foo";
$m->content_ref(\$foo);
ok($m->content("bar"), "foo");
ok($foo, "bar");
ok($m->content, "bar");
ok($m->content_ref, \$foo);

$m = HTTP::Message->new;
$m->content("fo=6F");
ok($m->decoded_content, "fo=6F");
$m->header("Content-Encoding", "quoted-printable");
ok($m->decoded_content, "foo");

$m = HTTP::Message->new;
$m->header("Content-Encoding", "gzip, base64");
$m->content_type("text/plain; charset=UTF-8");
$m->content("H4sICFWAq0ECA3h4eAB7v3u/R6ZCSUZqUarCoxm7uAAZKHXiEAAAAA==\n");

$@ = "";
skip($] < 5.008 ? "No Encode module" : "",
     sub { eval { $m->decoded_content } }, "\x{FEFF}Hi there \x{263A}\n");
ok($@ || "", "");
ok($m->content, "H4sICFWAq0ECA3h4eAB7v3u/R6ZCSUZqUarCoxm7uAAZKHXiEAAAAA==\n");

my $tmp = MIME::Base64::decode($m->content);
$m->content($tmp);
$m->header("Content-Encoding", "gzip");
$@ = "";
skip($] < 5.008 ? "No Encode module" : "",
     sub { eval { $m->decoded_content } }, "\x{FEFF}Hi there \x{263A}\n");
ok($@ || "", "");
ok($m->content, $tmp);

$m->header("Content-Encoding", "foobar");
ok($m->decoded_content, undef);
ok($@ =~ /^Don't know how to decode Content-Encoding 'foobar'/);

my $err = 0;
eval {
    $m->decoded_content(raise_error => 1);
    $err++;
};
ok($@ =~ /Don't know how to decode Content-Encoding 'foobar'/);
ok($err, 0);
