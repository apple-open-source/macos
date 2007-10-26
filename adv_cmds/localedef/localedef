#!/usr/bin/perl -w

use strict;
use Getopt::Std;
use Fcntl qw(O_TRUNC O_CREAT O_WRONLY SEEK_SET);
use File::Temp qw(tempfile);
use IO::File;

my %opt;
getopts("cf:u:i:", \%opt);

my $comment_char = "#";
my $escape_char = "\\";
my $val_match = undef;  # set in set_escape
my %sym = ();
my %width = ();
my %ctype_classes = (
	# there are the charactors that get automagically included, there is no
	# standard way to avoid them.  XXX even if you have a charset without
	# some of these charactors defined!

	# They are accessable in a regex via [:classname:], and libc has a
	# isX() for most of these.
	upper => {map { ($_, 1); } qw(A B C D E F G H I J K L M N O P Q R S T U V W X Y Z)},
	lower => {map { ($_, 1); } qw(a b c d e f g h i j k l m n o p q r s t u v w x y z)},
	alpha => {},
	#alnum => {},
	digit => {map { ($_, 1); } qw(0 1 2 3 4 5 6 7 8 9)},
	space => {},
	cntrl => {},
	punct => {},
	graph => {},
	print => {},
	xdigit => {map { ($_, 1); } qw(0 1 2 3 4 5 6 7 8 9 A B C D E F a b c d e f)},
	blank => {" " => 1, "\t" => 1},

	toupper => {map { ($_, "\U$_"); } qw(a b c d e f g h i j k l m n o p q r s t u v w x y z)},
	tolower => {map { ($_, "\L$_"); } qw(A B C D E F G H I J K L M N O P Q R S T U V W X Y Z)},
);

my %cele = (
	# collating-elements  -- these are a lot like %sym that only works
	# in LC_COLLATE, can also be accessed in a regex via [.element.]
);

my %csym = (
	# collating-symbols -- these are used to define a set of charactors
	# that compare as equals (in one or more passes), can also be accessed
	# in a regex via [=symbol=]
);

my @corder = (); # collating order
my @corder_weights = (); # collating directions (forward, backward, position)

my @colldef = ();

my(%monetary, %numeric, %time, %messages);

# This is the default charmap, unlike %ctype_classes you _can_ avoid this
# merely by having your own charmap definition file
my $default_charmap = <<EOT;
CHARMAP
<NUL>	 \\000
<alert>	 \\007
<backspace>	 \\010
<tab>	 \\011
<newline>	 \\012
<vertical-tab>	 \\013
<form-feed>	 \\014
<carriage-return>	 \\015
<space>	 \\040
<exclamation-mark>	 \\041
<quotation-mark>	 \\042
<number-sign>	 \\043
<dollar-sign>	 \\044
<percent-sign>	 \\045
<ampersand>	 \\046
<apostrophe>	 \\047
<left-parenthesis>	 \\050
<right-parenthesis>	 \\051
<asterisk>	 \\052
<plus-sign>	 \\053
<comma>	 \\054
<hyphen>	 \\055
<hyphen-minus>	 \\055
<period>	 \\056
<full-stop>	 \\056
<slash>	 \\057
<solidus>	 \\057
<zero>	 \\060
<one>	 \\061
<two>	 \\062
<three>	 \\063
<four>	 \\064
<five>	 \\065
<six>	 \\066
<seven>	 \\067
<eight>	 \\070
<nine>	 \\071
<colon>	 \\072
<semicolon>	 \\073
<less-then-sign>	 \\074
<less-than-sign>	 \\074
<equals-sign>	 \\075
<greater-then-sign>	 \\076
<greater-than-sign>	 \\076
<question-mark>	 \\077
<commercial-at>	 \\100
<A>	 \\101
<B>	 \\102
<C>	 \\103
<D>	 \\104
<E>	 \\105
<F>	 \\106
<G>	 \\107
<H>	 \\110
<I>	 \\111
<J>	 \\112
<K>	 \\113
<L>	 \\114
<M>	 \\115
<N>	 \\116
<O>	 \\117
<P>	 \\120
<Q>	 \\121
<R>	 \\122
<S>	 \\123
<T>	 \\124
<U>	 \\125
<V>	 \\126
<W>	 \\127
<X>	 \\130
<Y>	 \\131
<Z>	 \\132
<left-square-bracket>	 \\133
<backslash>	 \\134
<reverse-solidus>	 \\134
<right-square-bracket>	 \\135
<circumflex>	 \\136
<circumflex-accent>	 \\136
<underscore>	 \\137
<underline>	 \\137
<low-line>	 \\137
<grave-accent>	 \\140
<a>	 \\141
<b>	 \\142
<c>	 \\143
<d>	 \\144
<e>	 \\145
<f>	 \\146
<g>	 \\147
<h>	 \\150
<i>	 \\151
<j>	 \\152
<k>	 \\153
<l>	 \\154
<m>	 \\155
<n>	 \\156
<o>	 \\157
<p>	 \\160
<q>	 \\161
<r>	 \\162
<s>	 \\163
<t>	 \\164
<u>	 \\165
<v>	 \\166
<w>	 \\167
<x>	 \\170
<y>	 \\171
<z>	 \\172
<left-brace>	 \\173
<left-curly-bracket>	 \\173
<vertical-line>	 \\174
<right-brace>	 \\175
<right-curly-bracket>	 \\175
<tilde>	 \\176
<DEL>	 \\177

<SOH> \\x01
<STX> \\x02
<ETX> \\x03
<EOT> \\x04
<ENQ> \\x05
<ACK> \\x06
<BEL> \\x07
<BS> \\x08
<HT> \\x09
<NL> \\x0a
<VT> \\x0b
<NP> \\x0c
<CR> \\x0d
<SO> \\x0e
<SI> \\x0f
<DLE> \\x10
<DC1> \\x11
<DC2> \\x12
<DC3> \\x13
<DC4> \\x14
<NAK> \\x15
<SYN> \\x16
<ETB> \\x17
<CAN> \\x18
<EM> \\x19
<SUB> \\x1a
<ESC> \\x1b
<FS> \\x1c
<IS4> \\x1c
<GS> \\x1d
<IS3> \\x1d
<RS> \\x1e
<IS2> \\x1e
<US> \\x1f
<IS1> \\x1f
END CHARMAP
EOT

&set_escape($escape_char);

use strict qw(vars);

if (@ARGV != 1) {
	&exit(4, "usage: $0 [-c] [-f charmap-file] [-u codesetname] [-i localdef-file] LOCALENAME\n");
}

my $locale_dir = $ARGV[0];
$locale_dir = "/usr/share/locale/$locale_dir" unless ($locale_dir =~ m{/});

my $CMAP;
if (defined($opt{'f'})) {
	# Using new IO::File $opt{'f'}, "r" runs into problems with long path names
	sysopen(CMAP_KLUDGE, $opt{'f'}, O_RDONLY) || &exit(4, "Can't open $opt{f}: $!\n");
	$CMAP = new IO::Handle;
	$CMAP->fdopen(fileno(CMAP_KLUDGE), "r") || &exit(4, "Can't fdopen $opt{f}: $!\n");
} else {
	# er, not everyone gets IO::Scalar, so use an unamed tmp file
	# $CMAP = new IO::Scalar \$default_charmap;
	$CMAP = new_tmpfile IO::File;
	print $CMAP $default_charmap;
	seek $CMAP, 0, SEEK_SET;
}

while(<$CMAP>) {
	if (m/^\s*CHARMAP\s*$/) {
		&parse_charmaps();
	} elsif (m/^\s*WIDTH\s*$/) {
		&parse_widths();
	} elsif (m/^\s*($comment_char.*)?$/) {
	} else {
		chomp;
		&exit(4, "syntax error on line $. ($_)");
	}
}
&parse_widths() if (0 == %width);

if (defined($opt{'i'})) {
	sysopen(STDIN, $opt{'i'}, 0) || &exit(4, "Can't open localdef file $opt{i}: $!");
} else {
	$opt{'i'} = "/dev/stdin";
}

my %LC_parsers = (
	NONE => [\&parse_LC_NONE, qr/^\s*((escape|comment)_char\s+$val_match\s*)?$/],
	CTYPE => [\&parse_LC_CTYPE, qr/^\s*(\S+)\s+(\S+.*?)\s*$/],
	COLLATE => [\&parse_LC_COLLATE, qr/^\s*(<[^>\s]+>|order_end|END|(\S*)\s+(\S+.*?)|collating[_-]element\s*<[^>]+>\s+from\s+$val_match)\s*$/, 1],
	TIME => [\&parse_LC_TIME, qr/^\s*(ab_?day|day|abmon|mon|d_t_fmt|d_fmt|t_fmt|am_pm|t_fmt_ampm|era|era_d_fmt|era_t_fmt|era_d_t_fmt|alt_digits|copy|END)\s+(\S+.*?)\s*$/],
	NUMERIC => [\&parse_LC_NUMERIC, qr/^\s*(decimal_point|thousands_sep|grouping|END|copy)\s+(\S+.*?)\s*$/],
	MONETARY => [\&parse_LC_MONETARY, qr/^\s*(int_curr_symbol|currency_symbol|mon_decimal_point|mon_thousands_sep|mon_grouping|positive_sign|negative_sign|int_frac_digits|frac_digits|p_cs_precedes|p_sep_by_space|n_cs_precedes|n_sep_by_space|p_sign_posn|n_sign_posn|int_p_cs_precedes|int_n_cs_precedes|int_p_sep_by_space|int_n_sep_by_space|int_p_sign_posn|int_n_sign_posn|copy|END)\s+(\S+.*?)\s*$/],
	MESSAGES => [\&parse_LC_MESSAGES, qr/^\s*(END|yesexpr|noexpr|yesstr|nostr|copy)\s+(\S+.*?)\s*$/],
	"COLLATE order" => [\&parse_collate_order, qr/^\s*(order_end|(<[^>\s]+>|UNDEFINED|\Q...\E)(\s+\S+.*)?)\s*$/],
);
my($current_LC, $parse_func, $validate_line, $call_parse_on_END) 
  = ("NONE", $LC_parsers{"NONE"}->[0], $LC_parsers{"NONE"}->[1], undef);

while(<STDIN>) {
	next if (m/^\s*($comment_char.*)?\s*$/);
	if (m/\Q$escape_char\E$/) {
		chomp;
		chop;
		my $tmp = <STDIN>;
		if (!defined($tmp)) {
			&exit(4, "Syntax error, last line ($.) of $opt{i} is marked as a continued line\n");
		}
		$tmp =~ s/^\s*//;
		$_ .= $tmp;
		redo;
	}

	if ($current_LC eq "NONE" && m/^\s*LC_([A-Z]+)\s*$/) {
		&set_parser($1);
		next;
	}
	
	unless (m/$validate_line/) {
		&exit(4, "Syntax error on line $. of $opt{i}\n");
	}

	my($action, $args);
	if (m/^\s*(\S*)(\s+(\S+.*?))?\s*$/) {
		($action, $args) = ($1, $3);
	} else {
		$action = $_;
		chomp $action;
	}

	if ($action eq "END") {
		if ($args ne "LC_$current_LC" || $current_LC eq "NONE") {
			&exit(4, "Syntax error on line $. of $opt{i} attempting to end $args when LC_$current_LC is open\n");
		}
		if ($call_parse_on_END) {
		    &{$parse_func}($action, $args);
		}
		&set_parser("NONE");
	} else {
		&{$parse_func}($action, $args);
	}
}

mkdir($locale_dir);
&run_mklocale();
&write_lc_money();
&write_lc_time();
&write_lc_messages();
&write_lc_collate();
exit 0;

sub parse_charmaps {
	while(<$CMAP>) {
		# XXX need to parse out <code_set_name>, <mb_cur_max>, <mb_cur_min>,
		# <escape_char>, and <comment_char> before the generic "<sym> val"
		if (m/^\s*<([\w\-]+)>\s+($val_match+)\s*$/) {
			my($sym, $val) = ($1, $2);
			$val = &parse_value_double_backwhack($val);
			$sym{$sym} = $val;
		} elsif (m/^\s*<([\w\-]*\d)>\s*\Q...\E\s*<([\w\-]*\d)>\s+($val_match+)\s*$/) {
			# We don't deal with $se < $ss, or overflow of the last byte of $vs
			# then again the standard doesn't say anything in particular needs
			# to happen for those cases
			my($ss, $se, $vs) = ($1, $2, $3);
			$vs = &parse_value_double_backwhack($vs);
			my $vlast = length($vs) -1;
			for(my($s, $v) = ($ss, $vs); $s cmp $se; $s++) {
				$sym{$s} = $v;
				substr($v, $vlast) = chr(ord(substr($v, $vlast)) +1)
			}
		} elsif (m/^\s*END\s+CHARMAP\s*$/) {
			return;
		} elsif (m/^\s*($comment_char.*)?$/) {
		} else {
			&exit(4, "syntax error on line $.");
		}
	}
}

sub parse_widths {
	my $default = 1;
	my @syms;

	while(<$CMAP>) {
		if (m/^\s*<([\w\-]+)>\s+(\d+)\s*$/) {
			my($sym, $w) = ($1, $2);
			print "$sym width $w\n";
			if (!defined($sym{$sym})) {
				warn "localedef: can't set width of unknown symbol $sym on line $.\n";
			} else {
				$width{$sym} = $w;
			}
		} elsif (m/^\s*<([\w\-]+)>\s*\Q...\E\s*<([\w\-]+)>\s+(\d+)\s*$/) {
			my($ss, $se, $w) = ($1, $2, $3);
			if (!@syms) {
				@syms = sort { $a cmp $b } keys(%sym);
			}

			# Yes, we could do a binary search for find $ss in @syms
			foreach my $s (@syms) {
				if (($s cmp $ss) >= 0) {
					last if (($s cmp $se) > 0);
				}
			}
		} elsif (m/^\s*WIDTH_DEFAULT\s+(\d+)\s*$/) {
			$default = $1;
		} elsif (m/^\s*END\s+WIDTH\s*$/) {
			last;
		} elsif (m/^\s*($comment_char.*)?$/) {
		} else {
			&exit(4, "syntax error on line $.");
		}
	}

	foreach my $s (keys(%sym)) {
		if (!defined($width{$s})) {
			$width{$s} = $default;
		}
	}
}

# This parses a single value in any of the 7 forms it can appear in,
# returns [0] the parsed value and [1] the remander of the string
sub parse_value_return_extra {
	my $val = "";
	local($_) = $_[0];

	while(1) {
		$val .= &unsym($1), next
		  if (m/\G"((?:[^"\Q$escape_char\E]+|\Q$escape_char\E.)*)"/gc);
		$val .= chr(oct($1)), next
		  if (m/\G\Q$escape_char\E([0-7]+)/gc);
		$val .= chr(0+$1), next
		  if (m/\G\Q$escape_char\Ed([0-9]+)/gc);
		$val .= pack("H*", $1), next
		  if (m/\G\Q$escape_char\Ex([0-9a-fA-F]+)/gc);
		$val .= $1, next
		  if (m/\G([^,;<>\s\Q$escape_char()\E])/gc);
		$val .= $1
		  if (m/\G(?:\Q$escape_char\E)([,;<>\Q$escape_char()\E])/gc);
		$val .= &unsym($1), next
		  if (m/\G(<[^>]+>)/gc);

		m/\G(.*)$/;

		return ($val, $1);
	}
}

# Parse one value, if there is more then one value alert the media
sub parse_value {
	my ($ret, $err) = &parse_value_return_extra($_[0]);
	if ($err ne "") {
		&exit(4, "Syntax error, unexpected '$err' in value (after '$ret') on line $.\n");
	}

	return $ret;
}

sub parse_value_double_backwhack {
	my($val) = @_;

	my ($ret, $err) = &parse_value_return_extra($val);
	return $ret if ($err eq "");
	
	$val =~ s{\\\\}{\\}g;
	($ret, $err) = &parse_value_return_extra($val);
	if ($err ne "") {
		&exit(4, "Syntax error, unexpected '$err' in value (after '$ret') on line $.\n");
	}

	return $ret;
}
# $values is the string to parse, $dot_expand is a function ref that will
# return an array to insert when "X;...;Y" is parsed (undef means that
# construct is a syntax error), $nest is true if parens indicate a nested
# value string should be parsed and put in an array ref, $return_extra
# is true if any unparsable trailing junk should be returned as the last
# element (otherwise it is a syntax error).  Any text matching the regex 
# $specials is returned as an hash.
sub parse_values {
	my($values, $sep, $dot_expand, $nest, $return_extra, $specials) = @_;
	my(@ret, $live_dots);

	while($values ne "") {
		if (defined($specials) && $values =~ s/^($specials)($sep|$)//) {
			push(@ret, { $1, undef });
			next;
		}
		if ($nest && $values =~ s/^\(//) {
			my @subret = &parse_values($values, ',', $dot_expand, $nest, 1, $specials);
			$values = pop(@subret);
			push(@ret, [@subret]);
			unless ($values =~ s/^\)($sep)?//) {
				&exit(4, "Syntax error, unmatched open paren on line $. of $opt{i}\n");
			}
			next;
		}

		my($v, $l) = &parse_value_return_extra($values);
		$values = $l;

		if ($live_dots) {
			splice(@ret, -1, 1, &{$dot_expand}($ret[$#ret], $v));
			$live_dots = 0;
		} else {
			push(@ret, $v);
		}

		if (defined($dot_expand) && $values =~ s/^$sep\Q...\E$sep//) {
			$live_dots = 1;
		} elsif($values =~ s/^$sep//) {
			# Normal case
		} elsif($values =~ m/^$/) {
			last;
		} else {
			last if ($return_extra);
			&exit(4, "Syntax error parsing arguments on line $. of $opt{i}\n");
		}
	}

	if ($live_dots) {
		splice(@ret, -1, 1, &{$dot_expand}($ret[$#ret], undef));
	}
	if ($return_extra) {
		push(@ret, $values);
	}

	return @ret;
}

sub parse_LC_NONE {
	my($cmd, $arg) = @_;

	if ($cmd eq "comment_char") {
		$comment_char = &parse_value($arg);
	} elsif($cmd eq "escape_char") {
		&set_escape_char(&parse_value($arg));
	} elsif($cmd eq "") {
	} else {
		&exit(4, "Syntax error on line $. of $opt{i}\n");
	}
}

sub parse_LC_CTYPE {
	my($cmd, $arg) = @_;

	my $ctype_classes = join("|", keys(%ctype_classes));
	if ($cmd eq "copy") {
		# XXX -- the locale command line utility doesn't currently
		# output any LC_CTYPE info, so there isn't much of a way
		# to implent copy yet
		&exit(2, "copy not supported on line $. of $opt{i}\n");
	} elsif($cmd eq "charclass") {
		my $cc = &parse_value($arg);
		if (!defined($ctype_classes{$cc})) {
			$ctype_classes{$cc} = [];
		} else {
			warn "charclass $cc defined more then once\n";
		}
	} elsif($cmd =~ m/^to(upper|lower)$/) {
		my @arg = &parse_values($arg, ';', undef, 1);
		foreach my $p (@arg) {
			&exit(4, "Syntax error on line $. of $opt{i} ${cmd}'s arguments must be character pairs like (a,A);(b,B)\n") if ("ARRAY" ne ref $p || 2 != @$p);
		}
		foreach my $pair (@arg) {
			$ctype_classes{$cmd}{$pair->[0]} = $pair->[1];
		}
	} elsif($cmd =~ m/^($ctype_classes)$/) {
		my @arg = &parse_values($arg, ';', \&dot_expand, 0);
		foreach my $c (@arg) {
			$ctype_classes{$1}->{$c} = 1;
		}
	} elsif($cmd =~ "END") {
		&add_to_ctype_class('alpha', keys(%{$ctype_classes{'lower'}}));
		&add_to_ctype_class('alpha', keys(%{$ctype_classes{'upper'}}));
		foreach my $c (qw(alpha lower upper)) {
			foreach my $d (qw(cntrl digit punct space)) {
				&deny_in_ctype_class($c, $d, keys(%{$ctype_classes{$d}}));
			}
		}

		&add_to_ctype_class('space', keys(%{$ctype_classes{'blank'}}));
		foreach my $d (qw(upper lower alpha digit graph xdigit)) {
			&deny_in_ctype_class('space', $d, keys(%{$ctype_classes{$d}}));
		}

		foreach my $d (qw(upper lower alpha digit punct graph print xdigit)) {
			&deny_in_ctype_class('cntrl', $d, keys(%{$ctype_classes{$d}}));
		}
		
		foreach my $d (qw(upper lower alpha digit cntrl xdigit space)) {
			&deny_in_ctype_class('punct', $d, keys(%{$ctype_classes{$d}}));
		}
		
		foreach my $c (qw(graph print)) {
			foreach my $a (qw(upper lower alpha digit xdigit punct)) {
				&add_to_ctype_class($c, keys(%{$ctype_classes{$a}}));
			}
			foreach my $d (qw(cntrl)) {
				&deny_in_ctype_class($c, $d, keys(%{$ctype_classes{$d}}));
			}
		}
		&add_to_ctype_class('print', keys(%{$ctype_classes{'space'}}));

		# Yes, this is a requirment of the standard
		&exit(2, "The digit class must have exactly 10 elements\n") if (10 != values(%{$ctype_classes{'digit'}}));
		foreach my $d (values %{$ctype_classes{'digit'}}) {
			if (!defined $ctype_classes{'xdigits'}->{$d}) {
				&exit(4, "$d isn't in class xdigits, but all digits must appaer in xdigits\n");
			}
		}

		$ctype_classes{'alnum'} = {} unless defined $ctype_classes{'alnum'};
		foreach my $a (qw(alpha digit)) {
			&add_to_ctype_class('alnum', keys(%{$ctype_classes{$a}}));
		}
		
	} else {
		&exit(4, "Syntax error on line $. of $opt{i}\n");
	}
}

sub parse_LC_COLLATE {
    my ($cmd, $arg) = @_;
    if (defined($arg) && $arg ne "") {
	push(@colldef, "$cmd $arg");
    } else {
	push(@colldef, "$cmd");
    }
}

sub parse_collate_order {
	my($cmd, $arg) = @_;

	if ($cmd =~ m/order[-_]end/) {
		# restore the parent parser
		&set_parser("COLLATE");
		my $undef_at;
		for(my $i = 0; $i <= $#corder; ++$i) {
			next unless "ARRAY" eq ref($corder[$i]);
			# If ... appears as the "key" for a order entry it means the
			# rest of the line is duplicated once for everything in the
			# open ended range (key-pev-line, key-next-line).  Any ...
			# in the weight fields are delt with by &fixup_collate_order_args
			if ($corder[$i]->[0] eq "...") {
				my(@sym, $from, $to);

				my @charset = sort { $sym{$a} cmp $sym{$b} } keys(%sym);
				if ($i != 0) {
					$from = $corder[$i -1]->[0];
				} else {
					$from = $charset[0];
				}
				if ($i != $#corder) {
					$to = $corder[$i +1]->[0];
				} else {
					$to = $charset[$#charset];
				}

				my @expand;
				my($s, $e) = (&parse_value($from), &parse_value($to));
				foreach my $c (@charset) {
					if (($sym{$c} cmp $s) > 0) {
						last if (($sym{$c} cmp $e) >= 0);
						my @entry = @{$corder[$i]};
						$entry[0] = "<$c>";
						push(@expand, \@entry);
					}
				}
				splice(@corder, $i, 1, @expand);
			} elsif($corder[$i]->[0] eq "UNDEFINED") {
				$undef_at = $i;
				next;
			}
			&fixup_collate_order_args($corder[$i]);
		}

		if ($undef_at) {
			my @insert;
			my %cused = map { ("ARRAY" eq ref $_) ? ($_->[0], undef) : () } @corder;
			foreach my $s (keys(%sym)) {
				next if (exists $cused{"<$s>"});
				my @entry = @{$corder[$undef_at]};
				$entry[0] = "<$s>";
				&fixup_collate_order_args(\@entry);
				push(@insert, \@entry);
			}
			splice(@corder, $undef_at, 1, @insert);
		}
	} elsif((!defined $arg) || $arg eq "") {
		if (!exists($csym{$cmd})) {
			my($decode, $was_sym) = &unsym_with_check($cmd);
			if ($was_sym) {
				my %dots = ( "..." => undef );
				my @dots = (\%dots) x (0+@corder_weights);
				push(@corder, [$cmd, @dots]);
			} else {
				warn "Undefined collation symbol $cmd used on line $. of $opt{i}\n";
			}
		} else {
			push(@corder, $cmd);
		}
	} else {
		unless (defined($cele{$cmd} || defined $sym{$cmd})) {
			warn "Undefined collation element or charset sym $cmd used on line $. of $opt{i}\n";
		} else {
			# This expands all the symbols (but not colating elements), which
			# makes life easier for dealing with ..., but harder for
			# outputing the actual table at the end where we end up
			# converting literal sequences back into symbols in some cases
			my @args = &parse_values($arg, ';', undef, 0, 0,
			  qr/IGNORE|\Q...\E/);

			if (@args != @corder_weights) {
				if (@args < @corder_weights) {
					&exit(4, "Only " . (0 + @args) 
					  . " weights supplied on line $. of $opt{i}, needed "
					  . (0 + @corder_weights)
					  . "\n");
				} else {
					&exit(4,  "Too many weights supplied on line $. of $opt{i},"
					  . " wanted " . (0 + @corder_weights) . " but had "
					  . (0 + @args)
					  . "\n");
				}
			}

			push(@corder, [$cmd, @args]);
		}
	}
}

sub parse_LC_MONETARY {
	my($cmd, $arg) = @_;

	if ($cmd eq "copy") {
		&do_copy(&parse_value($arg));
	} elsif($cmd eq "END") {
	} elsif($cmd eq "mon_grouping") {
		my @v = &parse_values($arg, ';', undef, 0);
		$monetary{$cmd} = \@v;
	} else {
		my $v = &parse_value($arg);
		$monetary{$cmd} = $v;
	}
}

sub parse_LC_MESSAGES {
	my($cmd, $arg) = @_;

	if ($cmd eq "copy") {
		&do_copy(&parse_value($arg));
	} elsif($cmd eq "END") {
	} else {
		my $v = &parse_value($arg);
		$messages{$cmd} = $v;
	}
}

sub parse_LC_NUMERIC {
	my($cmd, $arg) = @_;

	if ($cmd eq "copy") {
		&do_copy(&parse_value($arg));
	} elsif($cmd eq "END") {
	} elsif($cmd eq "grouping") {
		my @v = &parse_values($arg, ';', undef, 0);
		$numeric{$cmd} = \@v;
	} else {
		my $v = &parse_value($arg);
		$numeric{$cmd} = $v;
	}
}

sub parse_LC_TIME {
	my($cmd, $arg) = @_;

	$cmd =~ s/^ab_day$/abday/;

	if ($cmd eq "copy") {
		&do_copy(&parse_value($arg));
	} elsif($cmd eq "END") {
	} elsif($cmd =~ m/abday|day|mon|abmon|am_pm|alt_digits/) {
		my @v = &parse_values($arg, ';', undef, 0);
		$time{$cmd} = \@v;
	} elsif($cmd eq "era") {
		my @v = &parse_values($arg, ':', undef, 0);
		$time{$cmd} = \@v;
	} else {
		my $v = &parse_value($arg);
		$time{$cmd} = $v;
	}
}


###############################################################################

sub run_mklocale {
	my $L = (new IO::File "|/usr/bin/mklocale -o $locale_dir/LC_CTYPE") || &exit(5, "$0: Can't start mklocale $!\n");
	if (defined($opt{'u'})) {
		$L->print(qq{ENCODING "$opt{u}"\n});
	} else {
		if ($ARGV[0] =~ m/(big5|euc|gb18030|gb2312|gbk|mskanji|utf-8)/i) {
		    my $enc = uc($1);
		    $L->print(qq{ENCODING "$enc"\n});
		} elsif($ARGV[0] =~ m/utf8/) {
		    $L->print(qq{ENCODING "UTF-8"\n});
		} else {
		    $L->print(qq{ENCODING "NONE"\n});
		}
	}
	foreach my $class (keys(%ctype_classes)) {
		unless ($class =~ m/^(tolower|toupper|alpha|control|digit|grah|lower|space|upper|xdigit|blank|print|ideogram|special|phonogram)$/) {
			$L->print("# skipping $class\n");
			next;
		}

		if (!%{$ctype_classes{$class}}) {
			$L->print("# Nothing in \U$class\n");
			next;
		}

		if ($class =~ m/^to/) {
			my $t = $class;
			$t =~ s/^to/map/;
			$L->print("\U$t ");

			foreach my $from (keys(%{$ctype_classes{$class}})) {
				$L->print("[", &hexchars($from), " ",
				  &hexchars($ctype_classes{$class}->{$from}), "] ");
			}
		} else {
			$L->print("\U$class ");

			foreach my $rune (keys(%{$ctype_classes{$class}})) {
				$L->print(&hexchars($rune), " ");
			}
		}
		$L->print("\n");
	}

	my @width;
	foreach my $s (keys(%width)) {
		my $w = $width{$s};
		$w = 3 if ($w > 3);
		push(@{$width[$w]}, &hexchars($sym{$s}));
	}
	for(my $w = 0; $w <= $#width; ++$w) {
		next if (!defined $width[$w]);
		next if (0 == @{$width[$w]});
		$L->print("SWIDTH$w ", join(" ", @{$width[$w]}), "\n");
	}

	if (!$L->close()) {
		if (0 == $!) {
			&exit(5, "Bad return from mklocale $?");
		} else {
			&exit(5, "Couldn't close mklocale pipe: $!");
		}
	}
}

###############################################################################

sub hexchars {
	my($str) = $_[0];
	my($ret);

	$ret = unpack "H*", $str;
	&exit(2, "Rune >4 bytes ($ret; for $str)") if (length($ret) > 8);

	return "0x" . $ret;
}

sub hexseq {
	my($str) = $_[0];
	my($ret);

	$ret = unpack "H*", $str;
	$ret =~ s/(..)/\\x$1/g;

	return $ret;
}

# dot_expand in the target charset
sub dot_expand {
	my($s, $e) = @_;
	my(@ret);

	my @charset = sort { $a cmp $b } values(%sym);
	foreach my $c (@charset) {
		if (($c cmp $s) >= 0) {
			last if (($c cmp $e) > 0);
			push(@ret, $c);
		}
	}

	return @ret;
}

# Convert symbols into literal values
sub unsym {
	my @ret = &unsym_with_check(@_);
	return $ret[0];
}

# Convert symbols into literal values (return[0]), and a count of how
# many symbols were converted (return[1]).
sub unsym_with_check {
	my($str) = $_[0];

	my $rx = join("|", keys(%sym));
	return ($str, 0) if ($rx eq "");
	my $found = $str =~ s/<($rx)>/$sym{$1}/eg;

	return ($str, $found);
}

# Convert a string of literals back into symbols.  It is an error
# for there to be literal values that can't be mapped back.  The
# converter uses a gredy algo.  It is likely this could be done
# more efficently with a regex ctrated at runtime.  It would also be
# a good idea to only create %rsym if %sym changes, but that isn't
# the simplest thing to do in perl5.
sub resym {
	my($str) = $_[0];
	my(%rsym, $k, $v);
	my $max_len = 0;
	my $ret = "";

	while(($k, $v) = each(%sym)) {
		# Collisions in $v are ok, we merely need a mapping, not the
		# identical mapping
		$rsym{$v} = $k;
		$max_len = length($v) if (length($v) > $max_len);
	}
	
	SYM: while("" ne $str) {
		foreach my $l ($max_len .. 1) {
			next if ($l > length($str));
			my $s = substr($str, 0, $l);
			if (defined($rsym{$s})) {
				$ret .= "<" . $rsym{$s} . ">";
				substr($str, 0, $l) = "";
				next SYM;
			}
		}
		&exit(4, "Can't convert $str ($_[0]) back into symbolic form\n");
	}

	return $ret;
}

sub set_escape {
	$escape_char = $_[0];
	$val_match = qr/"(?:[^"\Q$escape_char\E]+|\Q$escape_char\E")+"|(?:\Q$escape_char\E(?:[0-7]+|d[0-9]+|x[0-9a-fA-F]+))|[^,;<>\s\Q$escape_char\E]|(?:\Q$escape_char\E)[,;<>\Q$escape_char\E]/;
}

sub set_parser {
	my $section = $_[0];
	($current_LC, $parse_func, $validate_line, $call_parse_on_END) 
	  = ($section, $LC_parsers{$section}->[0], $LC_parsers{$section}->[1],
	  $LC_parsers{$section}->[2]);
	unless (defined $parse_func) {
		&exit(4, "Unknown section name LC_$section on line $. of $opt{i}\n");
	}
}

sub do_copy {
	my($from) = @_;
	local($ENV{LC_ALL}) = $from;

	my $C = (new IO::File "/usr/bin/locale -k LC_$current_LC |") || &exit(5, "can't fork locale during copy of LC_$current_LC");
	while(<$C>) {
		if (s/=\s*$/ ""/ || s/=/ /) {
			if (m/$validate_line/ && m/^\s*(\S*)(\s+(\S+.*?))?\s*$/) {
				my($action, $args) = ($1, $3);
				&{$parse_func}($action, $args);
			} else {
				&exit(4, "Syntax error on line $. of locale -k output"
				  . " during copy $current_LC\n");
			}
		} else {
			&exit(4, "Ill-formed line $. from locale -k during copy $current_LC\n");
		}
	}
	$C->close() || &exit(5, "copying LC_$current_LC from $from failed");
}

sub fixup_collate_order_args {
	my $co = $_[0];

	foreach my $s (@{$co}[1..$#{$co}]) {
		if ("HASH" eq ref($s) && exists($s->{"..."})) {
			$s = $co->[0];
		}
	}
}

sub add_to_ctype_class {
	my($class, @runes) = @_;
	
	my $c = $ctype_classes{$class};
	foreach my $r (@runes) {
		$c->{$r} = 2 unless exists $c->{$r};
	}
}

sub deny_in_ctype_class {
	my($class, $deny_reason, @runes) = @_;

	my $c = $ctype_classes{$class};
	foreach my $r (@runes) {
		next unless exists $c->{$r};
		$deny_reason =~ s/^(\S+)$/can't belong in class $class and in class $1 at the same time/;
		&exit(4, &hexchars($r) . " " . $deny_reason . "\n");
	}
}

# write_lc_{money,time,messages} all use the existing Libc format, which
# is raw text with each record terminated by a newline, and records
# in a predetermined order.

sub write_lc_money {
	my $F = (new IO::File "$locale_dir/LC_MONETARY", O_TRUNC|O_WRONLY|O_CREAT, 0666) || &exit(4, "$0 can't create $locale_dir/LC_MONETARY: $!");
	foreach my $s (qw(int_curr_symbol currency_symbol mon_decimal_point mon_thousands_sep mon_grouping positive_sign negative_sign int_frac_digits frac_digits p_cs_precedes p_sep_by_space n_cs_precedes n_sep_by_space p_sign_posn n_sign_posn int_p_cs_precedes int_n_cs_precedes int_p_sep_by_space int_n_sep_by_space int_p_sign_posn int_n_sign_posn)) {
		if (exists $monetary{$s}) {
			my $v = $monetary{$s};
			if ("ARRAY" eq ref $v) {
				$F->print(join(";", @$v), "\n");
			} else {
				$F->print("$v\n");
			}
		} else {
			if ($s =~ m/^(int_curr_symbol|currency_symbol|mon_decimal_point|mon_thousands_sep|positive_sign|negative_sign)$/) {
				$F->print("\n");
			} else {
				$F->print("-1\n");
			}
		}
	}
}

sub write_lc_time {
	my $F = (new IO::File "$locale_dir/LC_DATE", O_TRUNC|O_WRONLY|O_CREAT, 0666) || &exit(4, "$0 can't create $locale_dir/LC_DATE: $!");
	my %array_cnt = (abmon => 12, mon => 12, abday => 7, day => 7, alt_month => 12, am_pm => 2);

	$time{"md_order"} = "md" unless defined $time{"md_order"};

	foreach my $s (qw(abmon mon abday day t_fmt d_fmt d_t_fmt am_pm d_t_fmt mon md_order t_fmt_ampm)) {
		my $cnt = $array_cnt{$s};
		my $v = $time{$s};

		if (defined $v) {
			if (defined $cnt) {
				my @a = @{$v};
				&exit(4, "$0: $s has " . (0 + @a) 
				  . " elements, it needs to have exactly $cnt\n") 
				  unless (@a == $cnt);
				$F->print(join("\n", @a), "\n");
			} else {
				$F->print("$v\n");
			}
		} else {
			$cnt = 1 if !defined $cnt;
			$F->print("\n" x $cnt);
		}
	}
}

sub write_lc_messages {
	mkdir("$locale_dir/LC_MESSAGES");
	my $F = (new IO::File "$locale_dir/LC_MESSAGES/LC_MESSAGES", O_TRUNC|O_WRONLY|O_CREAT, 0666) || &exit(4, "$0 can't create $locale_dir/LC_DATE: $!");

	foreach my $s (qw(yesexpr noexpr yesstr nostr)) {
		my $v = $messages{$s};

		if (defined $v) {
			$F->print("$v\n");
		} else {
			$F->print("\n");
		}
	}
}

sub bylenval {
	return 0 if ("ARRAY" ne ref $a || "ARRAY" ne ref $b);

	my($aval, $af) = &unsym_with_check($a->[0]);
	$aval = $cele{$a->[0]} unless $af;
	my($bval, $bf) = &unsym_with_check($b->[0]);
	$bval = $cele{$b->[0]} unless $bf;

	my $r = length($aval) - length($bval);
	return $r if $r;
	return $aval cmp $bval;
}

sub write_lc_collate {
    return unless @colldef;

    # colldef doesn't parse the whole glory of SuSv3 charmaps, and we
    # already have, so we cna spit out a simplifyed one; unfortunitly
    # it doesn't like "/dev/fd/N" so we need a named tmp file
    my($CMAP, $cmapname) = tempfile(DIR => "/tmp");
    foreach my $s (keys(%sym)) {
	$CMAP->print("<$s>\t", sprintf "\\x%02x\n", ord($sym{$s}));
    }
    $CMAP->flush();
    unshift(@colldef, qq{charmap $cmapname});
    unshift(@colldef, "LC_COLLATE");
    $colldef[$#colldef] = "END LC_COLLATE";

    # Can't just use /dev/stdin, colldef appears to use seek,
    # and even seems to need a named temp file (re-open?)
    my($COL, $colname) = tempfile(DIR => "/tmp");
    $COL->print(join("\n", @colldef), "\n");
    $COL->flush();

    my $rc = system(
      "/usr/bin/colldef -o $locale_dir/LC_COLLATE $colname");
    unlink $colname, $cmapname;
    if ($rc) {
	&exit(1, "Bad return from colldef $rc");
    }
}

# Pack an int of unknown size into a series of bytes, each of which
# contains 7 bits of data, and the top bit is clear on the last
# byte of data.  Also works on arrays -- does not encode the size of
# the array.  This format is great for data that tends to have fewer
# then 21 bits.
sub pack_p_int {
	if (@_ > 1) {
		my $ret = "";
		foreach my $v (@_) {
			$ret .= &pack_p_int($v);
		}

		return $ret;
	}

	my $v = $_[0];
	my $b;

	&exit(4, "pack_p_int only works on positive values") if ($v < 0);
	if ($v < 128) {
		$b = chr($v);
	} else {
		$b = chr(($v & 0x7f) | 0x80);
		$b .= pack_p_int($v >> 7);
	}
	return $b;
}

sub strip_angles {
	my $s = $_[0];
	$s =~ s/^<(.*)>$/$1/;
	return $s;
}

# For localedef
#  xc=0 "no warnings, locale defined"
#  xc=1 "warnings, locale defined"
#  xc=2 "implmentation limits or unsupported charactor sets, no locale defined"
#  xc=3 "can't create new locales"
#  xc=4+ "wornings or errors, no locale defined"
sub exit {
    my($xc, $message) = @_;

    print STDERR $message;
    exit $xc;
}
