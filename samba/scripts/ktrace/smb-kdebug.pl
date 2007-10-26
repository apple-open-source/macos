#! /usr/bin/perl

# Copyright (C) 2007 Apple Inc. All rights reserved.

# smb-kdebug.pl : Generate trace.codes output for ktrace facility. Feed
# kdebug.names in on stdin to generate the kdebug trace codes used by smbd.

my $DBG_FUNC_START=	1;
my $DBG_FUNC_END=	2;
my $DBG_APPS          =  33;

sub KDBG_CODE
{
    my $Class= shift;
    my $SubClass= shift;
    my $code= shift;

    return ((($Class & 0xff) << 24) | (($SubClass & 0xff) << 16) | (($code & 0x3fff)  << 2))
}

sub APPSDBG_CODE
{
    my $SubClass = shift;
    my $code= shift;
    return KDBG_CODE($DBG_APPS, $SubClass, $code);
}

my $id = 0;
while (my $name = <>) {
    chomp $name;
    $name =~ s/^\s+//;
    printf "%#x\tSMB_%s\n", APPSDBG_CODE(128, $id), $name;
    ++$id;
}
