#!perl -w
# vim:ts=8:sw=4

use strict;

use Test::More;
use DBI;

BEGIN {
        plan skip_all => '$h->{Callbacks} attribute not supported for DBI::PurePerl'
                if $DBI::PurePerl && $DBI::PurePerl; # doubled to avoid typo warning
        plan tests => 35;
}

$| = 1;
my $dsn = "dbi:ExampleP:";
my %called;

ok my $dbh = DBI->connect($dsn, '', ''), "Create dbh";

is $dbh->{Callbacks}, undef, "Callbacks initially undef";
ok $dbh->{Callbacks} = my $cb = { };
is ref $dbh->{Callbacks}, 'HASH', "Callbacks can be set to a hash ref";
is $dbh->{Callbacks}, $cb, "Callbacks set to same hash ref";

$dbh->{Callbacks} = undef;
is $dbh->{Callbacks}, undef, "Callbacks set to undef again";

ok $dbh->{Callbacks} = {
    ping => sub {
	is $_, 'ping', '$_ holds method name';
	$called{$_}++;
	return;
    },
    quote_identifier => sub {
	is @_, 4, '@_ holds 4 values';
	my $dbh = shift;
	is ref $dbh, 'DBI::db', 'first is $dbh';
	is $_[0], 'foo';
	is $_[1], 'bar';
	is $_[2], undef;
	$_[2] = { baz => 1 };
	is $_, 'quote_identifier', '$_ holds method name';
	$called{$_}++;
	return (1,2,3);	# return something
    },
};
is keys %{ $dbh->{Callbacks} }, 2;

is ref $dbh->{Callbacks}->{ping}, 'CODE';

$_ = 42;
ok $dbh->ping;
is $called{ping}, 1;
is $_, 42, '$_ not altered by callback';

ok $dbh->ping;
is $called{ping}, 2;

my $attr;
eval { $dbh->quote_identifier('foo','bar', $attr) };
is $called{quote_identifier}, 1;
ok $@, 'quote_identifier callback caused fatal error';
is ref $attr, 'HASH', 'param modified by callback - not recommended!';

$dbh->{Callbacks} = undef;
ok $dbh->ping;
is $called{ping}, 2;

=for comment XXX

The big problem here is that conceptually the Callbacks attribute
is applied to the $dbh _during_ the $drh->connect() call, so you can't
set a callback on "connect" on the $dbh because connect isn't called
on the dbh, but on the $drh.

So a "connect" callback would have to be defined on the $drh, but that's
cumbersome for the user and then it would apply to all future connects
using that driver.

The best thing to do is probably to special-case "connect", "connect_cached"
and (the already special-case) "connect_cached.reused".

=cut

my @args = (
    $dsn, '', '', {
        Callbacks => {
            "connect_cached.new"    => sub { $called{new}++; return; },
            "connect_cached.reused" => sub { $called{cached}++; return; },
        }
    }
);

%called = ();

ok $dbh = DBI->connect(@args), "Create handle with callbacks";
is keys %called, 0, 'no callback for plain connect';

ok $dbh = DBI->connect_cached(@args), "Create handle with callbacks";
is $called{new}, 1, "connect_cached.new called";
is $called{cached}, undef, "connect_cached.reused not yet called";

ok $dbh = DBI->connect_cached(@args), "Create handle with callbacks";
is $called{cached}, 1, "connect_cached.reused called";
is $called{new}, 1, "connect_cached.new not called again";

