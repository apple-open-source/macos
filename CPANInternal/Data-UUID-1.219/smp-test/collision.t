#!/usr/bin/perl -w

my $cnt = shift || 1000;

my $collision = 0;

for (1..$cnt) {
    my $foo = `$^X -l -Mblib smp-test/uuid-fork.pl`;
    my @ret = ($foo =~ m/^(.*)$/mg);
    ++$collision#, print "==> collision ($foo)\n"
	if $ret[0] eq $ret[1];
}

print sprintf("%5.3f %% collision\n", $collision*100/$cnt);
