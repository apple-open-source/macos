#!/usr/local/bin/perl5
#
# CGI script for translating manpage into html on the fly.
# Front-end for PolyglotMan (formerly called RosettaMan)
#
# Author: Youki Kadobayashi  <youki@center.osaka-u.ac.jp>
#
# NOTE: Replace 'CGI::Apache' with just 'CGI' for systems without mod_perl.
#
# ALSO: You may want to recompile rman like this:
#	MANREFPRINTF = "/mod-bin/cgi-rman.pl?keyword=%s&section=%s"
#

use CGI::Apache font;			# for people with mod_perl and apache
## use CGI font;			# for people without mod_perl
$par = "<P>\n";
$brk = "<BR>\n";
$bg = '#c0ffff';

$query = new CGI::Apache;		# for people with mod_perl and apache
## $query = new CGI;			# for people without mod_perl

%mandatory = ('keyword' => 'Name or keyword',
	      'section' => 'Manual page section');

@given = $query->param;
if ($#given < 0) {
	&request_page;
	exit 0;
}

foreach $field (keys %mandatory) {
	if ($query->param($field) eq '') {
		push(@missing, $mandatory{$field});
	}
}
if ($#missing >= 0) {
    &info_missing_page(@missing);
} else {
    if ($query->param('type') eq 'apropos') {
	&apropos_page;
    } else {
	&manual_page;
    }
}
exit 0;

sub standout {
    my ($level, $color, $string) = @_;

    # As per CGI.pm documentation "Generating new HTML tags"
    return $query->font({color => "$color"}, "<$level>$string</$level>");
}

sub error_page {
    my ($message) = @_;

#    print $query->header,
#		$query->start_html(-title=>$message, -BGCOLOR=>$bg);

    print &standout("H2", "brown", $message),
		"The above error occured during the manual page generation",
		" process.  Please check keyword and section number,",
		" then try again.", $par;

#    print $query->end_html;
}

sub info_missing_page {
    my (@missing) = @_;

    print $query->header,
		$query->start_html(-title=>"Information is missing",
				   -BGCOLOR=>$bg);

    print &standout("H2", "brown", "Information is missing"),
		"Sorry but your request was not fulfilled because",
		" the following information is missing in your entry:", $par,
		join(' ', @missing), $par,
		"Please go back and make sure to enter data on the missing field.";

    print $query->end_html;
}

sub request_page {
    print $query->header,
		$query->start_html(-title=>'Hypertext Manual Page',
				   -author=>'Youki Kadobayashi',
				   -BGCOLOR=>$bg);

    print &standout("H2", "green", "Hypertext Manual Page");

    print $query->start_form,
	"Type of Search: ",
		$query->radio_group(-name=>'type',
				    -values=>['man', 'apropos'],
				    -default=>'man'), $brk,
	"Name or Keyword: ",
		$query->textfield(-name=>'keyword'), $brk,
	"Manpage Section: ",
		$query->popup_menu(-name=>'section',
				   -labels=>{
				       0 => 'All Sections',
				       1 => '1 - General Commands',
				       2 => '2 - System Calls',
				       3 => '3 - Library Functions',
				       4 => '4 - Device Special Files',
				       5 => '5 - File Formats',
				       6 => '6 - Games',
				       7 => '7 - Macros and Conventions',
				       8 => '8 - System Administration',
				       'pgsql' => 'PostgreSQL',
				       'tcl' => 'Tcl/Tk',
				       'mh' => 'Message Handler',
				       'isode' => 'ISODE',
				       'X11' => 'X Window System'},
				   -values=>[0, 1, 2, 3, 4, 5, 6, 7, 8,
					     'pgsql', 'tcl', 'mh',
					     'isode', 'X11'],
				   -default=>0), $brk;

    print $query->submit(-name=>'Submit'), $query->reset,
		$query->end_form, $par;

    print $query->end_html;
}

sub manual_page {
    my $keyword = $query->param('keyword');
    my $section = $query->param('section');
    my $man = "man";
    my $ok = "-a-zA-Z0-9._";

    # sanitize for security
    $section =~ s/[^$ok]+/_/go;
    $keyword =~ s/[^$ok]+/_/go;

    if ($section =~ /^[A-Za-z]/) {
	$man .= " -M$section";
    }
    elsif ($section =~ /^\d/) {
	$section =~ s/^(\d)\w*/\1/go;
	$man .= " -s $section";
    }
    $man .= ' ' . $keyword;
    open (MAN, "$man | /usr/local/bin/rman -f html -n $keyword -s $section |");

    print $query->header;
	# start_html and end_html not needed here, since rman tacks them.
    print "<!-- text generated with '$man' by cgi-rman.pl -->\n";
    while (<MAN>) {
	print $_;
    }
    if ($? != 0 || $. < 15) {
	&error_page("Your request for manual page '$keyword' failed.");
#	print "return code $?  line $.\n";
	close(MAN);
	return;
    }
    close(MAN);
}

sub apropos_page {
    my $keyword = $query->param('keyword');
    my $section = $query->param('section');	# igored
    my $man = "man -k";
    my $matches = 0;

    $man .= ' ' . $keyword;
    open (MAN, "$man |");

    $url = $query->url;
    print $query->header, $query->start_html(-title=>$man);

    while (<MAN>) {
	if (/^([a-zA-Z0-9_.-]+)[, ][a-zA-Z0-9_., -]*\((\d)\w*\)/) {
	    print $`, qq{ <A HREF="$url?keyword=$1&section=$2&type=man"> },
		$&, "</A>", $';
	    ++$matches;
	} else {
	    print $_;
	}
	print $brk;
    }
    if ($? != 0 || $matches == 0) {
	&error_page("Your search request with keyword '$keyword' failed.");
#	print "return code $?  matches $matches\n";
	close(MAN);
	return;
    }
    close(MAN);

    print $query->end_html;
}

