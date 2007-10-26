#! /usr/bin/perl -w
# Author: Thomas E. Dickey
# $XTermId: gen-pc-fkeys.pl,v 1.5 2005/04/03 16:58:29 tom Exp $
# $XFree86: xc/programs/xterm/gen-pc-fkeys.pl,v 1.2 2005/03/29 04:00:32 tsi Exp $
#
# Construct a list of function-key definitions corresponding to xterm's
# Sun/PC keyboard.  This uses infocmp to obtain the strings to modify (and
# verify).
use strict;

my(@old_keys);
my($min_fkeys,$max_fkeys,$max_modifier,$modify_opt,$terminfo);

$min_fkeys=12;		# the number of "real" function keys on your keyboard
$max_fkeys=64;		# the number of function-keys terminfo can support
$max_modifier=8;	# modifier 1 + (1=shift, 2=alt, 4=control 8=meta)
$modify_opt=2;		# xterm's modifyCursorKeys resource
$terminfo="xterm-new";	# the terminfo entry to use

my($cur_modifier, $cur_fkey);

# apply the given modifier to the terminfo string, return the result
sub modify_it {
	my $code = $_[0];
	my $text = $_[1];
	if ($code != 1) {
		my $piece = substr $text, 0, length ($text) - 1;
		my $final = substr $text, length ($text) - 1;
		my $check = substr $piece, length ($piece) - 1;
		if ($check =~ /[0-9]/) {
			$code = ";" . $code;
		}
		$text = $piece . $code . $final;
	}
	return $text;
}

# compute the next modifier value
sub next_modifier {
	my $code = $_[0];
	my $mask = $code - 1;
	if ($mask == 0) {
		$mask = 1;
	} elsif ($mask == 1) {
		$mask = 4;
	} elsif ($mask == 2) {
		$mask = 3;	# FIXME
	} elsif ($mask == 4) {
		$mask = 5;
	} elsif ($mask == 5) {
		$mask = 2;
	}
	# printf ("# next_modifier(%d) = %d\n", $code, $mask + 1);
	return $mask + 1;
}

# Read the terminfo entry's list of function keys $old_keys[].
# We could handle $old_keys[0], but choose to start numbering from 1.
sub readterm() {
	my($key,$n,$str);
	my(@list) = `infocmp -1 $terminfo`;
	for $n (0..$#list) {
		chop $list[$n];
		$list[$n] =~ s/^[[:space:]]//;
		if ( $list[$n] =~ /^kf[[:digit:]]+=/ ) {
			$key = $list[$n];
			$key =~ s/^kf//;
			$key =~ s/=.*//;
			$str = $list[$n];
			$str =~ s/^kf[[:digit:]]+=//;
			$str =~ s/,[[:space:]]*$//;
			# printf "$n:%s(%d)(%s)\n", $list[$n], $key, $str;
			$old_keys[$key] = $str;
		}
	}
	# printf ("last index:%d\n", $#old_keys);
}

readterm();

# Cycling through the modifiers is not just like counting.  Users prefer
# pressing one modifier (even if using Emacs).  So first we cycle through
# the individual modifiers, then for completeness two, three, etc.
printf "xterm+pcfkeys|fragment for PC-style keys, \n";
for ($cur_fkey = 1, $cur_modifier = 1; $cur_fkey < $max_fkeys; ++$cur_fkey) {
	my $index = (($cur_fkey - 1) % $min_fkeys);
	if ($index == 0 && $cur_fkey != 1) {
		$cur_modifier = next_modifier($cur_modifier);
	}
	my $input = $old_keys[$index + 1];
	my $result = modify_it($cur_modifier,$input);
	printf "\tkf%d=%s, \n", $cur_fkey, $result;
	if (defined $old_keys[$cur_fkey]) {
		if ($old_keys[$cur_fkey] ne $result) {
			printf "# diff %s\n", $old_keys[$cur_fkey];
		}
	}
}
