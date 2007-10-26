print "1..6\n";

use LWP::UserAgent;

$ua = LWP::UserAgent->new();

$ua->protocols_forbidden(['hTtP']);
print "not " unless scalar(@{$ua->protocols_forbidden()}) == 1;
print "ok 1\n";

print "not " unless @{$ua->protocols_forbidden()}[0] eq 'hTtP';
print "ok 2\n";

$response = $ua->get('http://www.cpan.org/');

print "not " unless $response->is_error();
print "ok 3\n";

print "not " if $ua->is_protocol_supported('http');
print "ok 4\n";

print "not " if $ua->protocols_allowed();
print "ok 5\n";

$ua->protocols_forbidden(undef);

print "not " if $ua->protocols_forbidden();
print "ok 6\n";
