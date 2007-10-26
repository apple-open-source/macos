#!perl -w

print "1..11\n";

use URI;
use strict;

my $u = URI->new('sip:phone@domain.ext');
print "not " unless $u->user eq 'phone' &&
		    $u->host eq 'domain.ext' &&
		    $u->port eq '5060' &&
		    $u eq 'sip:phone@domain.ext';
print "ok 1\n";

$u->host_port('otherdomain.int:9999');
print "not " unless $u->host eq 'otherdomain.int' &&
		    $u->port eq '9999' &&
		    $u eq 'sip:phone@otherdomain.int:9999';
print "ok 2\n";

$u->port('5060');
$u = $u->canonical;
print "not " unless $u->host eq 'otherdomain.int' &&
		    $u->port eq '5060' &&
		    $u eq 'sip:phone@otherdomain.int';
print "ok 3\n";

$u->user('voicemail');
print "not " unless $u->user eq 'voicemail' &&
		    $u eq 'sip:voicemail@otherdomain.int';
print "ok 4\n";

$u = URI->new('sip:phone@domain.ext?Subject=Meeting&Priority=Urgent');
print "not " unless $u->host eq 'domain.ext' &&
		    $u->query eq 'Subject=Meeting&Priority=Urgent';
print "ok 5\n";

$u->query_form(Subject => 'Lunch', Priority => 'Low');
my @q = $u->query_form;
print "not " unless $u->host eq 'domain.ext' &&
		    $u->query eq 'Subject=Lunch&Priority=Low' &&
		    @q == 4 && "@q" eq "Subject Lunch Priority Low";
print "ok 6\n";

$u = URI->new('sip:phone@domain.ext;maddr=127.0.0.1;ttl=16');
print "not " unless $u->host eq 'domain.ext' &&
		    $u->params eq 'maddr=127.0.0.1;ttl=16';
print "ok 7\n";

$u = URI->new('sip:phone@domain.ext?Subject=Meeting&Priority=Urgent');
$u->params_form(maddr => '127.0.0.1', ttl => '16');
my @p = $u->params_form;
print "not " unless $u->host eq 'domain.ext' &&
		    $u->query eq 'Subject=Meeting&Priority=Urgent' &&
		    $u->params eq 'maddr=127.0.0.1;ttl=16' &&
		    @p == 4 && "@p" eq "maddr 127.0.0.1 ttl 16";

print "ok 8\n";

$u = URI->new_abs('sip:phone@domain.ext', 'sip:foo@domain2.ext');
print "not " unless $u eq 'sip:phone@domain.ext';
print "ok 9\n";

$u = URI->new('sip:phone@domain.ext');
print "not " unless $u eq $u->abs('http://www.cpan.org/');
print "ok 10\n";

print "not " unless $u eq $u->rel('http://www.cpan.org/');
print "ok 11\n";
