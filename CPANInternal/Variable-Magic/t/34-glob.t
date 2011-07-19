#!perl -T

use strict;
use warnings;

use Test::More;

eval "use Symbol qw/gensym/";
if ($@) {
 plan skip_all => "Symbol::gensym required for testing magic for globs";
} else {
 plan tests => 2 * 8 + 1;
 diag "Using Symbol $Symbol::VERSION" if defined $Symbol::VERSION;
}

use Variable::Magic qw/cast dispell VMG_COMPAT_GLOB_GET/;

my %get = VMG_COMPAT_GLOB_GET ? (get => 1) : ();

use lib 't/lib';
use Variable::Magic::TestWatcher;

my $wiz = init_watcher
        [ qw/get set len clear free copy dup local fetch store exists delete/ ],
        'glob';

local *a = gensym();

watch { cast *a, $wiz } +{ }, 'cast';

watch { local *b = *a } +{ %get }, 'assign to';

watch { *a = gensym() } +{ %get, set => 1 }, 'assign';

watch {
 local *b = gensym();
 watch { cast *b, $wiz } +{ }, 'cast 2';
} +{ }, 'scope end';

watch { undef *a } +{ %get }, 'undef';

watch { dispell *a, $wiz } +{ %get }, 'dispell';
