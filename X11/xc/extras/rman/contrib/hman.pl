#!/usr/bin/perl -w
#!/citi/gtfd/mach/bin/perl -w
###############
# $Id: hman.pl,v 1.7 1997/11/04 21:56:13 teto Exp teto $
# $Source: /pub/src/dev/hman/RCS/hman.pl,v $
############################################
# TODO:
#	reorganize location of man pages - move 3x stuff from man3 to man3x
#	recurse - if 'man 3x curses' does not work try 'man 3 curses'
#	display more STDERR
#	Fix broken whatis entries - instead of
#		manpage section - description
#		manpage description section -
#	highlite keywords found
#	pass MANPATH as a command line argument
############################################
# Inspired by:
#	http://www.jinr.dubna.su/~gagin
#	http://thsun1.jinr.dubna.su/~gagin/rman.pl
#	http://thsun1.jinr.dubna.su/cgi-bin/rman.pl
#	http://www.jinr.dubna.su/~gagin/rman.pl.html
#
# CGI form interface to PolyglotMan program, which is available as
#	ftp://ftp.cs.berkeley.edu:/ucb/people/phelps/tcltk/rman.tar.Z
#
# The most recent version of this program available as
#	http://www.geocities.com/SiliconValley/Lakes/8777/hman.html
#---------------------------------------------------------------------
#  This is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
# 
#  hman is distributed in the hope that
#  it will be useful, but WITHOUT ANY WARRANTY; without even the implied
#  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#  See the GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this software; see the file COPYING.  If not, write to the
#  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.
# 
#  THIS SOFTWARE IS NOT DESIGNED OR INTENDED FOR USE OR RESALE AS ON-LINE
#  CONTROL EQUIPMENT IN HAZARDOUS ENVIRONMENTS REQUIRING FAIL-SAFE
#  PERFOHMANCE, SUCH AS IN THE OPERATION OF NUCLEAR FACILITIES, AIRCRAFT
#  NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL, DIRECT LIFE
#  SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH THE FAILURE OF THE
#  SOFTWARE COULD LEAD DIRECTLY TO DEATH, PERSONAL INJURY, OR SEVERE
#  PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH RISK ACTIVITIES").
#  ANY EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES
#  IS SPECIFICALLY DISCLAIMED.
#---------------------------------------------------------------------
# Request form: hman.pl?ManTopic=SOMETOPIC&ManSection=SECTION
# Request form: hman.pl?DirectPath=filename
#
#---------------------------------------------------------------------
# Stuff to change

# path to PolyglotMan program. "-b" is not nessesary
$hman="$ENV{HMANPRG} $ENV{HMANOPT}";

# path to man program
$ManPrg='/usr/bin/man';

# path to cat program
$ENV{PAGER} = '/bin/cat';

# path to man directories
($ManPathFind = $ENV{MANPATH}) =~ s/:/ /g;

# URL to this program
$hmanpl='/cgi-bin/hman';

# if man produced number of lines less then follows,
# I assume that request failed
$emptyman=5;

# tail of every produced html document
$HtmlTail='<hr><A HREF="' . $hmanpl . '">Back to Hman</a>';
# $HtmlTitle="<title>CGI form interface to PolyglotMan</title>\n";
$HtmlHdr="Content-type: text/html\n\n";

# end changable things
#----------------------------------------------------------------------
@ManSections = (
    '1', 'user commands',
    '2', 'system calls',
    '3', 'subroutines',
    '4', 'devices',
    '5', 'file formats',
    '6', 'games',
    '7', 'miscellanious',
    '8', 'sys. admin.',
    '9', 'Linux Internals',
    'n', 'section n'
);

#   Set unbuffered I/O.  Prevents buffering problems with
#   "system()" calls.
select((select(STDOUT), $| = 1)[0]);
print $HtmlHdr;

$string = $ENV{QUERY_STRING};
#
#	Initial Form
#
if($string eq "") { initialForm(); }

#
#	Generic parameter parsing ...
#
$DirectPath = $ManSection = $ManTopic = "";
$string =~ s/&/'; \$/g;
$string =~ s/=/='/g;
$string =~ s/^(.*)$/\$$1';/;
eval $string;

hmanDirect($DirectPath) if ($DirectPath ne "");

if ($ManTopic eq "") { badness("<code>Topic for man search needed</code>\n"); }

if ($ManSection eq "") { badness("<code>No section specified</code>\n"); }

$ManSection =~ s/all//;
if ($ManSection =~ /key/) { manKey($ManTopic); }

findIt($ManTopic);
open(MANOUT, "$ManPrg $ManSection $ManTopic |")
    || die "$hmanpl: can't run \"$ManPrg Section $ManTopic |\", $!\n";
for (0..$emptyman) {
    $temp = <MANOUT> || last;
    push @temp, $temp;
}
#
if (@temp < $emptyman) {
    close(MANOUT);
    print"<strong>Request failed for topic $ManTopic:</strong>\n<code>";
    for (@temp) {print;}
    print "</code><br><h3>Let's try a keyword search:</h3><hr>\n";
    manKey($ManTopic);
}
#
$cmd = "$hman -r \"$hmanpl?ManTopic=%s&ManSection=%s\" -l \"Man page for $ManTopic";
if ($ManSection eq "") {
    $cmd .= '"';
} else {
    $cmd .= "($ManSection)\"";
}
#
open(HMANIN, "| $cmd") || die "$hmanpl: can't open $cmd: $!\n";
for (@temp) {print HMANIN;}
while(<MANOUT>){print HMANIN;}
close(MANOUT);
close(HMANIN);

exitIt();
###############################################################################
sub initialForm {
    print <<EOF;
<h1>Select a manual page:</h1>
    <FORM METHOD="GET" action="$hmanpl">
	<table border=0>
	    <tr>
		<td align=right>Section:</td>
		<td>
		    <select name=ManSection>
			<OPTION value=all>All</option>
			<OPTION value=key>Keyword Search</option>
EOF
    for ($ndx = 0; $ndx < @ManSections; $ndx += 2) {
	print '			<OPTION value=' . $ManSections[$ndx] . '>'
	    . "$ManSections[$ndx] - $ManSections[$ndx + 1] "
	    . '</option>' . "\n";
    }
    print <<EOF;
		    </SELECT>
		</td>
	    </tr>
	    <tr>
		<td align=right>Topic:</td>
		<td><input type="TEXT" name="ManTopic"></td>
	    </tr>
	    <tr>
		<td align=right><INPUT TYPE="submit" VALUE="Search">
		<td align=left><input type="reset" value="Reset">
		</td>
	    </tr>
	</table>
    </FORM>
EOF
    exitIt();
}
sub findIt {
    my($topic) = ($_[0]);
    my($cmd, $mixedCase, $navigation, $zcat);
    $mixedCase = '';
    foreach (split(/\s*/, $topic)) {
	$mixedCase .= "[" . $_ . uc($_) . "]";
    }
    $cmd = 'find ' . $ManPathFind . ' \( -type f -o -type l \) -name '
	. $mixedCase .'\* | sort -t. +1';
    open(FINDIN, "$cmd |") || die "can't open pipe \"$cmd |\": $!";
    $navigation = 0;
    while (<FINDIN>) {
	if ($navigation == 0) {
	    print "<UL>\n";
	    $navigation = 1;
	}
	$_ =~ s/[\n\r]*$//;
	print "<li><A HREF=\"$hmanpl?DirectPath=$_\">$_</a>\n";
    }
    close(FINDIN);
    print "</UL><hr>\n" if ($navigation == 1);
}
sub hmanDirect {
    my($path) = ($_[0], $_[1]);
    my($cmd, $dir, $topic, $section);
    ($dir = $path) =~ s/\.(gz|z|Z)$//;
    ($topic = $dir) =~ s,^.*/,,;
    $dir =~ s,/[^/]*/[^/]*$,,;
    # $dir =~ s,/[^/]*$,,;
    ($section=$topic)=~s/^.*\.([^\.]*)$/$1/;
    $topic =~ s/\.[^\.]*$//;
    findIt($topic);
    $cmd = "cd $dir; (gzip -dc < $path 2>/dev/null || cat < $path) | $hman -r '"
	. $hmanpl . '?ManTopic=%s&ManSection=%s' ."' -l 'Man page for $topic($section)'";
    system($cmd) || warn "can't run command \"$cmd\": $!";
    print $HtmlTail;
    exit 0;
}
sub exitIt {
    print $HtmlTail;
    exit 0;
}
sub badness {
    my($text) = ($_[0]);
    print "<strong>Request failed:</strong> $text, Try again<hr>\n";
    initialForm();
}
sub manKey {
    my($topic) = ($_[0]);
    open(TMPHTML,"$ManPrg -k $topic | sort -u |")
	|| die "can't open pipe \"$ManPrg -k $topic | sort -u |\": $!\n";
    print "<title>Keyword search results for \"$topic\"</title>\n";
    print "<h1>Keyword search results for \"$topic\"</h1><hr>\n";
    while(<TMPHTML>) {
	s/\( \)//g;
	next if (! /^([^(]+)\s*\(([^)]+)[^-]+-\s(.*)[\n\r]*$/);
	@topics=split(/, /,$1);
	next if ($2 eq "");
	print "<h2>$3:</h2>\n";
	print "<UL>\n";
	for $topic (@topics) {
	    print "<li><A HREF=\"$hmanpl?ManSection=$2&ManTopic=$topic\">$topic</a>($2)\n";
	}
	print "</UL>\n";
    }
    close(TMPHTML);
    exitIt();
}
