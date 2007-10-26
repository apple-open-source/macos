#!perl
my $r = shift;

$r->send_http_header("text/plain");

my $err = $@{$r->prev->uri};

my $note = $r->prev->notes('error-notes') || 'NONE';

print "ServerError: $err\n";

if ($note eq $err) {
    print "error-notes is also set";
}
else {
    print "error-notes is different: $note";
}

print "\n";
print 'dump of %@:', "\n";
print map { "$_ = $@{$_}\n" } keys %{'@'};

