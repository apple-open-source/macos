g/'\(-arch [^ ]\+ \)\+ */s//'/g
g/=\(-arch [^ ]\+ \)\+ *'/s//='/g
g/=\(-arch [^ ]\+ \)\+-/s//=-/g
g/=\(-arch [^ ]\+ \)\+ \+/s//= /g
/^$_ =/s//my $_str1 =/
/^extras=/a
extrasarch='@EXTRASARCH@'
extraslib='@EXTRASLIB@'
.
/^installarchlib=/s,'.*','@UPDATESARCH@',
/^installbin/a
installextrasarch='@EXTRASARCH@'
installextraslib='@EXTRASLIB@'
.
/^installprivlib=/s,'.*','@UPDATESLIB@',
/^installusrbinperl/i
installupdatesarch='@UPDATESARCH@'
installupdateslib='@UPDATESLIB@'
.
/^uniq=/a
updatesarch='@UPDATESARCH@'
updateslib='@UPDATESLIB@'
.
?^local \*_ =?m/^our $byteorder =/
i
my $_archflags = exists($ENV{ARCHFLAGS}) ? $ENV{ARCHFLAGS} : '@ARCHFLAGS@';
my %_archkeys = (
    archflags => 1,
    ccflags => 1,
    ccflags_nolargefiles => 1,
    lddlflags => 1,
    ldflags => 1,
    ldflags_nolargefiles => 1,
);
my $_64bit = ((~0>>1) > 2147483647);
my $_64bitdefine = ($_64bit ? 'define' : 'undef');
my $_64bitsize = ($_64bit ? '8' : '4');
my $_64bitundef = ($_64bit ? 'undef' : 'define');

my %_change = (
    byteorder => $Config::byteorder,
    d_nv_preserves_uv => $_64bitundef,
    gidformat => ($_64bit ? '"u"' : '"lu"'),
    i32type => ($_64bit ? 'int' : 'long'),
    i64type => ($_64bit ? 'long' : 'long long'),
    ivsize => $_64bitsize,
    longsize => $_64bitsize,
    need_va_copy => $_64bitdefine,
    nv_preserves_uv_bits => ($_64bit ? '53' : '32'),
    ptrsize => $_64bitsize,
    quadkind => ($_64bit ? '2' : '3'),
    quadtype => ($_64bit ? 'long' : 'long long'),
    sizesize => $_64bitsize,
    u32type => ($_64bit ? 'unsigned int' : 'unsigned long'),
    u64type => ($_64bit ? 'unsigned long' : 'unsigned long long'),
    uidformat => ($_64bit ? '"u"' : '"lu"'),
    uquadtype => ($_64bit ? 'unsigned long' : 'unsigned long long'),
    use64bitall => $_64bitdefine,
    use64bitint => $_64bitdefine,
    uvsize => $_64bitsize,
);
if(exists($ENV{RC_XBS}) && $ENV{RC_XBS} eq 'YES') {
    $_change{installarchlib} = '@ARCHLIB@';
    $_change{installprivlib} = '@PRIVLIB@';
}

sub _fix {
    my $in = shift;
    my($k, $v);
    local $_;
    ($k, $_) = split('=', $in, 2);
    return $in unless defined($k);
    $_archkeys{$k} && do { s/(['"])/$1$_archflags /; return join('=', $k, $_); };
    defined($v = $_change{$k}) && do { s/(['"]).*?\1/$1$v$1/; return join('=', $k, $_); };
    $in;
}

.
/^s\/(byteorder=/c
$_ = $_part1 . "archflags='$_archflags'\n";
.
/^our $Config_SH_expanded =/a
.
.t/^EOVIRTUAL/
s/<<.*/$_part2;/
?^our $Config_SH_expanded =?a
.
.,/^EOVIRTUAL/m?^local \*_?-1
?^our $Config_SH_expanded = .*<<?s//my $_part2 = join("\\n", map(_fix($_), split("\\n", <</
s/;$/, -1)));/
i
my $_part1 = join("\n", map(_fix($_), split("\n", $_str1, -1)));
.
w!
