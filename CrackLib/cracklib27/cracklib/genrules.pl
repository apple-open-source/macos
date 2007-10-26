#!/usr/local/bin/perl

###
# This program is copyright Alec Muffett 1997. The author disclaims all
# responsibility or liability with respect to it's usage or its effect
# upon hardware or computer systems, and maintains copyright as set out
# in the "LICENCE" document which accompanies distributions of Crack v5.0
# and upwards.
###

%perms = ();

sub Permute
{
    my $depth = shift;
    my $i;

    if ($depth == 0)
    {
	@stack = ();
    }
    for ($i = 0; $i <= $#set; $i++)
    {
	$stack[$depth] = $set[$i];

	if ($depth == $#set)
	{
	    %output = ();
	    foreach (@stack) 
	    { 
		$output{$_}++; 
	    }
	    $perm = join(" ", sort(keys(%output)));
	    $perms{$perm}++;
	}
	else
	{
	    &Permute($depth + 1);
	}
    }
}

@maps = ([ '/$s$s', '/0s0o', '/2s2a', '/3s3e', '/5s5s', '/1s1i', '/4s4a' ],
	 [ '/$s$s', '/0s0o', '/2s2a', '/3s3e', '/5s5s', '/1s1i', '/4s4h' ],
	 [ '/$s$s', '/0s0o', '/2s2a', '/3s3e', '/5s5s', '/1s1l', '/4s4a' ],
	 [ '/$s$s', '/0s0o', '/2s2a', '/3s3e', '/5s5s', '/1s1l', '/4s4h' ]);

@set = (0 .. 5);

# why be elegant when you can use brute force?  moreover, it's easier
# to think brute-force at 1am in the morning, and leaves you with time
# for coffee...

&Permute(0);

foreach $perm (sort(keys(%perms)))
{
    foreach $aref (@maps)
    {
	foreach $i (split(" ", $perm))
	{
	    print ${$aref}[$i];
	}
	print "\n";
    }
}

exit 0;
