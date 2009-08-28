# $Id: 02-header.t 704 2008-02-06 21:30:59Z olaf $

use Test::More tests => 18;
use strict;

BEGIN { use_ok('Net::DNS'); }

my $header = Net::DNS::Header->new;

ok($header,                "new() returned something");

$header->id(41);
$header->qr(1);
$header->opcode('QUERY');
$header->aa(1);
$header->tc(0);
$header->rd(1);
$header->cd(0);
$header->ra(1);
$header->rcode("NOERROR");

$header->qdcount(1);
$header->ancount(2);
$header->nscount(3);
$header->arcount(3);

is($header->id,     41,       'id() works');
is($header->qr,     1,         'qr() works');
is($header->opcode, 'QUERY',   'opcode() works');
is($header->aa,     1,         'aa() works');
is($header->tc,     0,         'tc() works');
is($header->rd,     1,         'rd() works');
is($header->cd,     0,         'cd() works');
is($header->ra,     1,         'ra() works');
is($header->rcode,  'NOERROR', 'rcode() works');


my $data = $header->data;

my $header2 = Net::DNS::Header->parse(\$data);

is_deeply($header, $header2, 'Headers are the same');

#
#  Is $header->string remotely sane?
#
like($header->string, '/opcode = QUERY/', 'string() has opcode correct');
like($header->string, '/ancount = 2/',    'string() has ancount correct');

$header = Net::DNS::Header->new;

#
# Check that the aliases work properly.
#
$header->zocount(0);
$header->prcount(1);
$header->upcount(2);
$header->adcount(3);

is($header->zocount, 0, 'zocount works');
is($header->prcount, 1, 'prcount works');
is($header->upcount, 2, 'upcount works');
is($header->adcount, 3, 'adcount works');

