#!perl -w
use strict;

# $Id: signal.t,v 1.2 1997-05-15 23:23:59-04 roderick Exp $
#
# Copyright (c) 1997 Roderick Schertler.  All rights reserved.  This
# program is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.

BEGIN {
    $| = 1;
    print "1..7\n";
}

use IPC::Signal qw(/^/);

sub ok {
    my ($n, $result, @info) = @_;
    if ($result) {
    	print "ok $n\n";
    }
    else {
    	print "not ok $n\n";
	print "# ", @info, "\n" if @info;
    }
}

ok 1, @Sig_name == 0,			"name predefined: @Sig_name";
ok 2, keys %Sig_num == 0,		'num predefined';
ok 3, sig_num('HUP') == 1,		sig_num 'HUP';
ok 4, sig_name(1) eq 'HUP',		sig_name 1;
ok 5, keys %Sig_num >= @Sig_name,	keys(%Sig_num) . ' < ' . @Sig_name;
ok 6, $Sig_num{HUP} == 1,		$Sig_num{HUP};
ok 7, $Sig_name[1] eq 'HUP',		$Sig_name[1];
