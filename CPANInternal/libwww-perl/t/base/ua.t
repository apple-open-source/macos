print "1..10\n";

use LWP::UserAgent;

$ua = LWP::UserAgent->new;
$clone = $ua->clone;

print "not " unless $ua->agent =~ /^libwww-perl/;
print "ok 1\n";


print "not " if defined $ua->proxy(ftp => "http://www.sol.no");
print "ok 2\n";

print "not " unless $ua->proxy("ftp") eq "http://www.sol.no";
print "ok 3\n";

@a = $ua->proxy([qw(ftp http wais)], "http://proxy.foo.com");

for (@a) { $_ = "undef" unless defined; }

print "not " unless "@a" eq "http://www.sol.no undef undef";
print "ok 4\n";

print "not " unless $ua->proxy("http") eq "http://proxy.foo.com";
print "ok 5\n";

print "not " unless ref($ua->default_headers) eq "HTTP::Headers";
print "ok 6\n";

$ua->default_header("Foo" => "bar", "Multi" => [1, 2]);
print "not " unless $ua->default_headers->header("Foo") eq "bar";
print "ok 7\n";

print "not " unless $ua->default_header("Foo") eq "bar";
print "ok 8\n";

# Try it
$ua->proxy(http => "loopback:");
$ua->agent("foo/0.1");

print "not " unless $ua->get("http://www.example.com", x => "y")->content eq <<EOT; print "ok 9\n";
GET http://www.example.com
User-Agent: foo/0.1
Foo: bar
Multi: 1
Multi: 2
X: y

EOT

print "not " unless (ref($clone->{proxy}) eq 'HASH');
print "ok 10\n";
