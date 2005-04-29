# $Id: 03-question.t,v 1.1 2004/04/09 17:04:48 dasenbro Exp $

use Test::More tests => 11;
use strict;

BEGIN { use_ok('Net::DNS'); }


my $domain = 'example.com';
my $type   = 'MX';
my $class  = 'IN';

my $q = Net::DNS::Question->new($domain, $type, $class);

ok($q,                 'new() returned something.');

is($q->qname,  $domain, 'qname()'  );
is($q->qtype,  $type,   'qtype()'  );
is($q->qclass, $class,  'qclass()' );

#
# Check the aliases
#
is($q->zname,  $domain, 'zname()'  );
is($q->ztype,  $type,   'ztype()'  );
is($q->zclass, $class,  'zclass()' );

#
# Check that we can change stuff
#
$q->qname('example.net');
$q->qtype('A');
$q->qclass('CH');

is($q->qname,  'example.net', 'qname()'  );
is($q->qtype,  'A',           'qtype()'  );
is($q->qclass, 'CH',          'qclass()' );
