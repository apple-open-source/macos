#! /usr/bin/perl

# Grep a config.log lookiing for undefined symbols that
# are expected to be in libkrb5.dylib. 

use strict;

my $krb5lib = '/usr/lib/libkrb5.dylib';
my %syms;

if (open NM, "nm $krb5lib |") {
    while (my $line = <NM>) {
	chomp $line;
	my ($addr, $type, $name) = split / /, $line;

	if ($type =~ /[Tt]/) {
	    $syms{$name} = 1;
	}
    }

    close NM;
}

while (my $line = <>) {
    if ($line =~ m/Undefined symbols/) {
	my $symbol = <>;

	chomp $symbol;

	if (defined($syms{$symbol})) {
	    system ("nm -m $krb5lib | grep $symbol\n");
	}
    }
}

