#!perl -w

use strict;
use Test qw(plan ok);

plan tests => 122;

use HTML::Form;

my @warn;
$SIG{__WARN__} = sub { push(@warn, $_[0]) };

my @f = HTML::Form->parse("", "http://localhost/");
ok(@f, 0);

@f = HTML::Form->parse(<<'EOT', "http://localhost/");
<form action="abc" name="foo">
<input name="name">
</form>
<form></form>
EOT

ok(@f, 2);

my $f = shift @f;
ok($f->value("name"), "");
ok($f->dump, "GET http://localhost/abc [foo]\n  name=                          (text)\n");

my $req = $f->click;
ok($req->method, "GET");
ok($req->uri, "http://localhost/abc?name=");

$f->value(name => "Gisle Aas");
$req = $f->click;
ok($req->method, "GET");
ok($req->uri, "http://localhost/abc?name=Gisle+Aas");

ok($f->attr("name"), "foo");
ok($f->attr("method"), undef);

$f = shift @f;
ok($f->method, "GET");
ok($f->action, "http://localhost/");
ok($f->enctype, "application/x-www-form-urlencoded");
ok($f->dump, "GET http://localhost/\n");

# try some more advanced inputs
$f = HTML::Form->parse(<<'EOT', "http://localhost/");
<form method=post>
   <input name=i type="image" src="foo.gif">
   <input name=c type="checkbox" checked>
   <input name=r type="radio" value="a">
   <input name=r type="radio" value="b" checked>
   <input name=t type="text">
   <input name=p type="PASSWORD">
   <input name=h type="hidden" value=xyzzy>
   <input name=s type="submit" value="Doit!">
   <input name=r type="reset">
   <input name=b type="button">
   <input name=f type="file" value="foo.txt">
   <input name=x type="xyzzy">

   <textarea name=a>
abc
   </textarea>

   <select name=s>
      <option>Foo
      <option value="bar" selected>Bar
   </select>

   <select name=m multiple>
      <option selected value="a">Foo
      <option selected value="b">Bar
   </select>
</form>
EOT

#print $f->dump;
#print $f->click->as_string;

ok($f->click->as_string, <<'EOT');
POST http://localhost/
Content-Length: 76
Content-Type: application/x-www-form-urlencoded

i.x=1&i.y=1&c=on&r=b&t=&p=&h=xyzzy&f=foo.txt&x=&a=%0Aabc%0A+++&s=bar&m=a&m=b
EOT

ok(@warn, 1);
ok($warn[0] =~ /^Unknown input type 'xyzzy'/);
@warn = ();

$f = HTML::Form->parse(<<'EOT', "http://localhost/");
<form>
   <input type=submit value="Upload it!" name=n disabled>
   <input type=image alt="Foo">
   <input type=text name=t value="1">
</form>
EOT

#$f->dump;
ok($f->click->as_string, <<'EOT');
GET http://localhost/?x=1&y=1&t=1

EOT

# test file upload
$f = HTML::Form->parse(<<'EOT', "http://localhost/");
<form method=post enctype="MULTIPART/FORM-DATA">
   <input name=f type=file value=>
   <input type=submit value="Upload it!">
</form>
EOT

#print $f->dump;
#print $f->click->as_string;

ok($f->click->as_string, <<'EOT');
POST http://localhost/
Content-Length: 0
Content-Type: multipart/form-data; boundary=none

EOT

my $filename = sprintf "foo-%08d.txt", $$;
die if -e $filename;

open(FILE, ">$filename") || die;
binmode(FILE);
print FILE "This is some text\n";
close(FILE) || die;

$f->value(f => $filename);

#print $f->click->as_string;

ok($f->click->as_string, <<"EOT");
POST http://localhost/
Content-Length: 139
Content-Type: multipart/form-data; boundary=xYzZY

--xYzZY\r
Content-Disposition: form-data; name="f"; filename="$filename"\r
Content-Type: text/plain\r
\r
This is some text
\r
--xYzZY--\r
EOT

unlink($filename) || warn "Can't unlink '$filename': $!";

ok(@warn, 0);

# Try to parse form HTTP::Response directly
{
    package MyResponse;
    use vars qw(@ISA);
    require HTTP::Response;
    @ISA = ('HTTP::Response');

    sub base { "http://www.example.com" }
}
my $response = MyResponse->new(200, 'OK');
$response->content("<form><input type=text value=42 name=x></form>");

$f = HTML::Form->parse($response);

ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=42

EOT

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form>
   <input type=checkbox name=x> I like it!
</form>
EOT

$f->find_input("x")->check;

ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=on

EOT

$f->value("x", "off");
ok($f->click->as_string, <<"EOT");
GET http://www.example.com

EOT

$f->value("x", "I like it!");
ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=on

EOT

$f->value("x", "I LIKE IT!");
ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=on

EOT

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form>
<select name=x>
   <option value=1>one
   <option value=2>two
   <option>3
</select>
<select name=y multiple>
   <option value=1>
</select>
</form>
EOT

$f->value("x", "one");

ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=1

EOT

$f->value("x", "TWO");
ok($f->click->as_string, <<"EOT");
GET http://www.example.com?x=2

EOT

ok(join(":", $f->find_input("x")->value_names), "one:two:3");
ok(join(":", map $_->name, $f->find_input(undef, "option")), "x:y");

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form>
<input name=x value=1 disabled>
<input name=y value=2 READONLY type=TEXT>
<input name=z value=3 type=hidden>
</form>
EOT

ok($f->value("x"), 1);
ok($f->value("y"), 2);
ok($f->value("z"), 3);
ok($f->click->uri->query, "y=2&z=3");

my $input = $f->find_input("x");
ok($input->type, "text");
ok(!$input->readonly);
ok($input->disabled);
ok($input->disabled(0));
ok(!$input->disabled);
ok($f->click->uri->query, "x=1&y=2&z=3");

$input = $f->find_input("y");
ok($input->type, "text");
ok($input->readonly);
ok(!$input->disabled);

$input->value(22);
ok($f->click->uri->query, "x=1&y=22&z=3");
ok(@warn, 1);
ok($warn[0] =~ /^Input 'y' is readonly/);
@warn = ();

ok($input->readonly(0));
ok(!$input->readonly);

$input->value(222);
ok(@warn, 0);
print @warn;
ok($f->click->uri->query, "x=1&y=222&z=3");

$input = $f->find_input("z");
ok($input->type, "hidden");
ok($input->readonly);
ok(!$input->disabled);

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form>
<textarea name="t" type="hidden">
<foo>
</textarea>
<select name=s value=s>
 <option name=y>Foo
 <option name=x value=bar type=x>Bar
</form>
EOT

ok($f->value("t"), "\n<foo>\n");
ok($f->value("s"), "Foo");
ok(join(":", $f->find_input("s")->possible_values), "Foo:bar");
ok(join(":", $f->find_input("s")->other_possible_values), "bar");
ok($f->value("s", "bar"), "Foo");
ok($f->value("s"), "bar");
ok(join(":", $f->find_input("s")->other_possible_values), "");


$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form>

<input type=radio name=r0 value=1 disabled>one

<input type=radio name=r1 value=1 disabled>one
<input type=radio name=r1 value=2>two
<input type=radio name=r1 value=3>three

<input type=radio name=r2 value=1>one
<input type=radio name=r2 value=2 disabled>two
<input type=radio name=r2 value=3>three

<select name=s0>
 <option disabled>1
</select>

<select name=s1>
 <option disabled>1
 <option>2
 <option>3
</select>

<select name=s2>
 <option>1
 <option disabled>2
 <option>3
</select>

<select name=s3 disabled>
 <option>1
 <option disabled>2
 <option>3
</select>

<select name=m0 multiple>
 <option disabled>1
</select>

<select name=m1 multiple="">
 <option disabled>1
 <option>2
 <option>3
</select>

<select name=m2 multiple>
 <option>1
 <option disabled>2
 <option>3
</select>

<select name=m3 disabled multiple>
 <option>1
 <option disabled>2
 <option>3
</select>

</form>

EOT
#print $f->dump;
ok($f->find_input("r0")->disabled);
ok(!eval {$f->value("r0", 1);});
ok($@ && $@ =~ /^The value '1' has been disabled for field 'r0'/);
ok($f->find_input("r0")->disabled(0));
ok(!$f->find_input("r0")->disabled);
ok($f->value("r0", 1), undef);
ok($f->value("r0"), 1);

ok(!$f->find_input("r1")->disabled);
ok($f->value("r1", 2), undef);
ok($f->value("r1"), 2);
ok(!eval {$f->value("r1", 1);});
ok($@ && $@ =~ /^The value '1' has been disabled for field 'r1'/);

ok($f->value("r2", 1), undef);
ok(!eval {$f->value("r2", 2);});
ok($@ && $@ =~ /^The value '2' has been disabled for field 'r2'/);
ok(!eval {$f->value("r2", "two");});
ok($@ && $@ =~ /^The value 'two' has been disabled for field 'r2'/);
ok(!$f->find_input("r2")->disabled(1));
ok(!eval {$f->value("r2", 1);});
ok($@ && $@ =~ /^The value '1' has been disabled for field 'r2'/);
ok($f->find_input("r2")->disabled(0));
ok(!$f->find_input("r2")->disabled);
ok($f->value("r2", 2), 1);

ok($f->find_input("s0")->disabled);
ok(!$f->find_input("s1")->disabled);
ok(!$f->find_input("s2")->disabled);
ok($f->find_input("s3")->disabled);

ok(!eval {$f->value("s1", 1);});
ok($@ && $@ =~ /^The value '1' has been disabled for field 's1'/);

ok($f->find_input("m0")->disabled);
ok($f->find_input("m1", undef, 1)->disabled);
ok(!$f->find_input("m1", undef, 2)->disabled);
ok(!$f->find_input("m1", undef, 3)->disabled);

ok(!$f->find_input("m2", undef, 1)->disabled);
ok($f->find_input("m2", undef, 2)->disabled);
ok(!$f->find_input("m2", undef, 3)->disabled);

ok($f->find_input("m3", undef, 1)->disabled);
ok($f->find_input("m3", undef, 2)->disabled);
ok($f->find_input("m3", undef, 3)->disabled);

$f->find_input("m3", undef, 2)->disabled(0);
ok(!$f->find_input("m3", undef, 2)->disabled);
ok($f->find_input("m3", undef, 2)->value(2), undef);
ok($f->find_input("m3", undef, 2)->value(undef), 2);

$f->find_input("m3", undef, 2)->disabled(1);
ok($f->find_input("m3", undef, 2)->disabled);
ok(eval{$f->find_input("m3", undef, 2)->value(2)}, undef);
ok($@ && $@ =~ /^The value '2' has been disabled/);
ok(eval{$f->find_input("m3", undef, 2)->value(undef)}, undef);
ok($@ && $@ =~ /^The 'm3' field can't be unchecked/);

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<!-- from http://www.blooberry.com/indexdot/html/tagpages/k/keygen.htm -->
<form  METHOD="post" ACTION="http://example.com/secure/keygen/test.cgi" ENCTYPE="application/x-www-form-urlencoded">
   <keygen NAME="randomkey" CHALLENGE="1234567890">
   <input TYPE="text" NAME="Field1" VALUE="Default Text">
</form>
EOT

ok($f->find_input("randomkey"));
ok($f->find_input("randomkey")->challenge, "1234567890");
ok($f->find_input("randomkey")->keytype, "rsa");
ok($f->click->as_string, <<EOT);
POST http://example.com/secure/keygen/test.cgi
Content-Length: 19
Content-Type: application/x-www-form-urlencoded

Field1=Default+Text
EOT

$f->value(randomkey => "foo");
ok($f->click->as_string, <<EOT);
POST http://example.com/secure/keygen/test.cgi
Content-Length: 33
Content-Type: application/x-www-form-urlencoded

randomkey=foo&Field1=Default+Text
EOT

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form  ACTION="http://example.com/">
   <select name=s>
     <option>1
     <option>2
   <input name=t>
</form>
EOT

ok($f);
ok($f->find_input("t"));


@f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form  ACTION="http://example.com/">
   <select name=s>
     <option>1
     <option>2
</form>
<form  ACTION="http://example.com/">
     <input name=t>
</form>
EOT

ok(@f, 2);
ok($f[0]->find_input("s"));
ok($f[1]->find_input("t"));

$f = HTML::Form->parse(<<EOT, "http://www.example.com");
<form  ACTION="http://example.com/">
  <fieldset>
    <legend>Radio Buttons with Labels</legend>
    <label>
      <input type=radio name=r0 value=0 />zero
    </label>
    <label>one
      <input type=radio name=r1 value=1>
    </label>
    <label for="r2">two</label>
    <input type=radio name=r2 id=r2 value=2>
    <label>
      <span>nested</span>
      <input type=radio name=r3 value=3>
    </label>
    <label>
      before
      and <input type=radio name=r4 value=4>
      after
    </label>
  </fieldset>
</form>
EOT

ok(join(":", $f->find_input("r0")->value_names), "zero");
ok(join(":", $f->find_input("r1")->value_names), "one");
ok(join(":", $f->find_input("r2")->value_names), "two");
ok(join(":", $f->find_input("r3")->value_names), "nested");
ok(join(":", $f->find_input("r4")->value_names), "before and after");
