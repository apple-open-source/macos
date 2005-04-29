package Razor2::Preproc::deHTML;


sub new { 

    my $class = shift;

    my %html_tags = (
        "lt"    => '<',        "gt" => '>',           "amp" => '&',
        "quot"  => '"',        "nbsp" => ' ',         "iexcl" => chr(161),
        "cent"  => chr(162),   "pound" => chr(163),   "curren" => chr(164),
        "yen"   => chr(165),   "brvbar" => chr(166),  "sect" => chr(167),
        "uml"   => chr(168),   "copy" => chr(169),    "ordf" => chr(170),
        "laquo" => chr(171),   "not" => chr(172),     "shy" => chr(173),
        "reg"   => chr(174),   "macr" => chr(175),    "deg" => chr(176),
        "plusmn" => chr(177),  "sup2" => chr(178),    "sup3" => chr(179),
        "acute" => chr(180),   "micro" => chr(181),   "para" => chr(182),
        "middot" => chr(183),  "cedil" => chr(184),   "sup1" => chr(185),
        "ordm" => chr(186),    "raquo" => chr(187),   "frac14" => chr(188),
        "frac12" => chr(189),  "frac34" => chr(190),  "iquest" => chr(191),
        "Agrave" => chr(192),  "Aacute" => chr(193),  "Acirc" => chr(194),
        "Atilde" => chr(195),  "Auml" => chr(196),    "Aring" => chr(197),
        "AElig" => chr(198),   "Ccedil" => chr(199),  "Egrave" => chr(200),
        "Eacute" => chr(201),  "Ecirc" => chr(202),   "Euml" => chr(203),
        "Igrave" => chr(204),  "Iacute" => chr(205),  "Icirc" => chr(206),
        "Iuml" => chr(207),    "ETH" => chr(208),     "Ntilde" => chr(209),
        "Ograve" => chr(210),  "Oacute" => chr(211),  "Ocirc" => chr(212),
        "Otilde" => chr(213),  "Ouml" => chr(214),    "times" => chr(215),
        "Oslash" => chr(216),  "Ugrave" => chr(217),  "Uacute" => chr(218),
        "Ucirc" => chr(219),   "Uuml" => chr(220),    "Yacute" => chr(221),
        "THORN" => chr(222),   "szlig" => chr(223),   "agrave" => chr(224),
        "aacute" => chr(225),  "acirc" => chr(226),   "atilde" => chr(227),
        "auml" => chr(228),    "aring" => chr(229),   "aelig" => chr(230),
        "ccedil" => chr(231),  "egrave" => chr(232),  "eacute" => chr(233),
        "ecirc" => chr(234),   "euml" => chr(235),    "igrave" => chr(236),
        "iacute" => chr(237),  "icirc" => chr(238),   "iuml" => chr(239),
        "eth" => chr(240),     "ntilde" => chr(241),  "ograve" => chr(242),
        "oacute" => chr(243),  "ocirc" => chr(244),   "otilde" => chr(245),
        "ouml" => chr(246),    "divide" => chr(247),  "oslash" => chr(248),
        "ugrave" => chr(249),  "uacute" => chr(250),  "ucirc" => chr(251),
        "uuml" => chr(252),    "yacute" => chr(253),  "thorn" => chr(254),
        "yuml" => chr(255)
    );

    return bless {
        html_tags => \%html_tags,
    }, $class;

}


sub isit {

    my ($self, $text) = @_;
    my $isit = 0;
    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;

    return 0 unless $body;

    $isit = $body =~ /(?:<HTML>|<BODY|<FONT|<A HREF)/ism;
    return $isit if $isit;

    $isit = $hdr =~ m"^Content-Type: text/html"ism;
    return $isit;

}

# NOTE: Tag designations _are_ case sensitive.
# Returns: (length of tag detected, char)
# So the caller can basically do ``$i += $length_detected'', above.
#
# The big difference between this function and its C equivalent is
# that we're not modifying the char array that was passed in, but
# rather instead advising the caller on how much to skip/eat.

sub html_xlat_old {

    my($self, $chars, $i) = @_;
    my($tag, $val);
    my($r_val);
    my $r_tag = "";

    return 0 if ($$chars[$i] !~ /[a-zA-Z]/);

    # first figure out which is shorter, $i->EOS, or $i+10, and use
    # that to build a compare string for use with substr.  Otherwise,
    # requesting ``lengths'' from char arrays that exceeds the actual
    # array produce additional 'undef's.  10 is an arbitrary #, but at
    # least greater than the max length (+ 1) of any html &tag.

    my($offset) = (scalar @{$chars} - $i > 10 ? 10 : scalar @{$chars} );
    my($s) = join ('', @{$chars}[$i .. $i + $offset]);

    while ( ($tag, $val) = each %{$self->{html_tags}} ) {

        if (substr($s, 0, length($tag)) eq $tag) {
            ($r_tag, $r_val) = ($tag => $val);
            $r_tag .= ';' # so the length($r_tag) consumes the ``;''
               if (substr($s, length($tag), 1) eq ';');
        }
    }

    return (length($r_tag), $r_val);

}

sub html_xlat {
    my($self, $chars, $i) = @_;

    #print "html_xlat($r_tag) start\n";
    return 0 if ($$chars[$i] !~ /[a-zA-Z]/);

    my $r_tag;
    # we used to walk till we got a ';', but to be compatible
    # with c, we won't check for ';'
    while ($$chars[$i] && $$chars[$i] =~ /[a-zA-Z]/) {
        $r_tag .= $$chars[$i++];
    }
    my $len = length($r_tag);  # do not include ;
    $len++ if ($$chars[$i] eq ';');

    my $val = $self->{html_tags}->{$r_tag};
    #print "html_xlat($r_tag) = ($len,$val)\n";

    return 0 unless $val;  # not found 
    return ( $len, $val );
}

sub doit {

    my ($self, $text) = @_;
    my ($hdr, $body) = split /\n\r*\n/, $$text, 2;

    my(@chars) = split //, $body;
    my($len) = scalar @chars;

    my($last, $quote, $sgml, $tag) = ("", "", "", "");
    my(@out);
    my($i) = 0;

    while ($i < $len) {
	my($c) = $chars[$i++];

	if ($c eq $quote) {

	    if ($c eq '-' && $last ne '-') {
		$last = $c;
		next;
	    } else {
		$last = 0;
	    }

	    $quote = "";

	} elsif (!$quote) {

	    if ($c eq '<') {

		$tag = 1;
		if ($chars[$i++] eq '!') {
                    my($s) = join('', @chars[$i .. $i + 10]);
		    $sgml = 1;
		}
		
	    } elsif ($c eq '>') {
    
		if ($tag) {

		    $sgml = 0;
		    $tag = 0;
		}

	    } elsif ($c eq '-') {

		if ($sgml and $last eq '-') {

		    $quote = '-';
		} else {
		push @out, $c if (! $tag);
		}

	    } elsif ($c eq "\"" or $c eq "\'") {
		
		if ($tag) { 
                  $quote = $c ;
		} else {
		push @out, $c if (! $tag);
                }

	    } elsif ($c eq "&") {

		my($len, $char) = $self->html_xlat(\@chars, $i);
		if ($len) {
		    push @out, $char;
		    $i += $len;
		} else {
		    push @out, $c;
		}
		
	    } else { 

		push @out, $c if (! $tag);

	    }

	}

	$last = $c;
    }

    $$text = "$hdr\n\n". join('', @out);

}


1;

