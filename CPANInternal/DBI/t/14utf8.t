#!perl -w
# vim:ts=8:sw=4
$|=1;

use Test::More;
use DBI;

plan skip_all => "Requires perl 5.8"
    unless $] >= 5.008;

eval {
    require Storable;
    import Storable qw(dclone);
    require Encode;
    import Encode qw(_utf8_on _utf8_off is_utf8);
};

plan skip_all => "Unable to load required module ($@)"
    unless defined &_utf8_on;

plan tests => 16;

$dbh = DBI->connect("dbi:Sponge:foo","","", {
        PrintError => 0,
        RaiseError => 1,
});

my $source_rows = [ # data for DBD::Sponge to return via fetch
    [ 41,	"AAA",	9	],
    [ 42,	"BB",	undef	],
    [ 43,	undef,	7	],
    [ 44,	"DDD",	6	],
];

my($sth, $col0, $col1, $col2, $rows);

# set utf8 on one of the columns so we can check it carries through into the
# keys of fetchrow_hashref
my @col_names = qw(Col1 Col2 Col3);
_utf8_on($col_names[1]);
ok  is_utf8($col_names[1]);
ok !is_utf8($col_names[0]);

$sth = $dbh->prepare("foo", {
	rows => dclone($source_rows),
	NAME => \@col_names,
});

ok($sth->bind_columns(\($col0, $col1, $col2)) );
ok($sth->execute(), $DBI::errstr);

ok $sth->fetch;
cmp_ok $col1, 'eq', "AAA";
ok !is_utf8($col1);

# force utf8 flag on
_utf8_on($col1);
ok is_utf8($col1);

ok $sth->fetch;
cmp_ok $col1, 'eq', "BB";
# XXX sadly this test doesn't detect the problem when using DBD::Sponge
# because DBD::Sponge uses $sth->_set_fbav (correctly) and that uses
# sv_setsv which doesn't have the utf8 persistence that sv_setpv does.
ok !is_utf8($col1);	# utf8 flag should have been reset

ok $sth->fetch;
ok !defined $col1;	# null
ok !is_utf8($col1);	# utf8 flag should have been reset

ok my $hash = $sth->fetchrow_hashref;
ok 1 == grep { is_utf8($_) } keys %$hash;

$sth->finish;

# end
