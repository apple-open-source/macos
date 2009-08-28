g/'\(-arch [^ ]\+ \)\+ */s//'/g
g/=\(-arch [^ ]\+ \)\+ *'/s//='/g
g/=\(-arch [^ ]\+ \)\+-/s//=-/g
g/=\(-arch [^ ]\+ \)\+ \+/s//= /g
/^extras=/a
extrasarch='@EXTRASARCH@'
extraslib='@EXTRASPERL@'
.
/^installarchlib=/s,'.*','@UPDATESARCH@',
/^installbin/a
installextrasarch='@EXTRASARCH@'
installextraslib='@EXTRASPERL@'
.
/^installprivlib=/s,'.*','@ENV_UPDATESLIB@',
/^installusrbinperl/i
installupdatesarch='@UPDATESARCH@'
installupdateslib='@ENV_UPDATESLIB@'
.
/^uniq=/a
updatesarch='@UPDATESARCH@'
updateslib='@ENV_UPDATESLIB@'
.
/^sub fetch_string/-1i
my $archflags;

my %archkeys = (
    archflags => 1,
    ccflags => 1,
    lddlflags => 1,
    ldflags => 1,
    ccflags_nolargefiles => 1,
    ldflags_nolargefiles => 1,
);

my $sizeref;

my %size64 = (
    ivsize => '8',
    longsize => '8',
    ptrsize => '8',
    sizesize => '8',
    uvsize => '8',
);

my %size32 = (
    ivsize => '4',
    longsize => '4',
    ptrsize => '4',
    sizesize => '4',
    uvsize => '4',
);

.
/^sub fetch_string/+1a

    if(!defined($sizeref)) {
	# $s will be negative (begins with -) on 32-bit architectures;
	# it will be positive (begins with a 2) on 64-bit architectures.
	my $s = sprintf("%d", 2147483648);
	$sizeref = ($s =~ /^-/) ? \%size32 : \%size64;
    }
    my $size = $sizeref->{$key};
    return($self->{$key} = $sizeref->{$key}) if defined($size);

    my $origkey = $key;
    if(exists($ENV{RC_XBS}) && $ENV{RC_XBS} eq 'YES') {
	$key = 'privlib' if $key eq 'installprivlib';
	$key = 'archlib' if $key eq 'installarchlib';
    }
.
/# So we can say/i

    if($archkeys{$key}) {
	if(!defined($archflags)) {
	    $archflags = exists($ENV{ARCHFLAGS}) ? $ENV{ARCHFLAGS} : '@ARCHFLAGS@';
	    $archflags =~ s/^\s+//;
	    $archflags =~ s/\s+$//;
	}
	if($key eq 'archflags') {
	    $value = $archflags;
	} elsif($archflags ne '') {
	    $value = "$archflags $value";
	}
    }

.
/$self->{$key} = $value;/s/$key/$origkey/
w!
