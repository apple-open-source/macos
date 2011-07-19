#!perl -T

use strict;
use warnings;

use Test::More;

use Variable::Magic qw/wizard cast dispell VMG_UVAR/;

if (!VMG_UVAR) {
 plan skip_all => 'No nice uvar magic for this perl';
}

eval "use Hash::Util::FieldHash";
if ($@) {
 plan skip_all => 'Hash::Util::FieldHash required for testing uvar interaction';
} else {
 plan tests => 2 * 5 + 7 + 1;
 my $v = $Hash::Util::FieldHash::VERSION;
 diag "Using Hash::Util::FieldHash $v" if defined $v;
}

use lib 't/lib';
use Variable::Magic::TestWatcher;

my $wiz = init_watcher [ qw/fetch store/ ], 'huf';
ok defined($wiz),       'huf: wizard with uvar is defined';
is ref($wiz), 'SCALAR', 'huf: wizard with uvar is a scalar ref';

Hash::Util::FieldHash::fieldhash(\my %h);

my $obj = { };
bless $obj, 'Variable::Magic::Test::Mock';
$h{$obj} = 5;

my ($res) = watch { cast %h, $wiz } { }, 'cast uvar magic on fieldhash';
ok $res, 'huf: cast uvar magic on fieldhash succeeded';

my ($s) = watch { $h{$obj} } { fetch => 1 }, 'fetch on magical fieldhash';
is $s, 5, 'huf: fetch on magical fieldhash succeeded';

watch { $h{$obj} = 7 } { store => 1 }, 'store on magical fieldhash';
is $h{$obj}, 7, 'huf: store on magical fieldhash succeeded';

($res) = watch { dispell %h, $wiz } { }, 'dispell uvar magic on fieldhash';
ok $res, 'huf: dispell uvar magic on fieldhash succeeded';

$h{$obj} = 11;
$s = $h{$obj};
is $s, 11, 'huf: store/fetch on fieldhash after dispell still ok';

$Variable::Magic::TestWatcher::mg_end = { fetch => 1 };
