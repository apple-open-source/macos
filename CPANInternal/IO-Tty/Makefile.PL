use ExtUtils::MakeMaker;

use strict;
use IO::File;
use Config qw(%Config);

my $flags = "$Config{ccflags} $Config{ldflags}";
$flags =~ s/([^A-Za-z0-9 -])/\\$1/g; # escape shell-metachars

$|=1; # to see output immediately
$^W=1;

my %define;
my @libs;
my $Package_Version = '1.08';  # keep this consistent with Tty.pm
my $Is_Beta = ($Package_Version =~ m/_/);

open(SUB, ">xssubs.c") or die "open: $!";

warn "WARNING: perl versions prior to 5.8 are untested and may have problems.\n"
  if $] < 5.008;

# improve backward-compatibility
@define{qw(-DPL_sv_undef=sv_undef -DPL_dowarn=dowarn)} = (undef, undef)
  if $] < 5.004_05;

print <<_EOT_;
Now let's see what we can find out about your system
(logfiles of failing tests are available in the conf/ dir)...
_EOT_

#
# Now some poking around in /dev to see what we can find
#

@define{qw(-DHAVE_CYGWIN -DHAVE_DEV_PTMX)} = (undef, undef)
  if ($^O =~ m/cygwin/i);

$define{'-DHAVE_DEV_PTMX'} = undef
  if (-c '/dev/ptmx');

$define{'-DHAVE_DEV_PTYM_CLONE'} = undef
  if (-c '/dev/ptym/clone');

$define{'-DHAVE_DEV_PTC'} = undef
  if (-c "/dev/ptc");

$define{'-DHAVE_DEV_PTMX_BSD'} = undef
  if (-c "/dev/ptmx_bsd");

if (-d "/dev/ptym" and -d "/dev/pty") {
  $define{'-DHAVE_DEV_PTYM'} = undef;
  
}




# config tests go to a separate dir
unless( mkdir 'conf', 0777 ) {
  my $e = $!;
  die "mkdir: $e" unless -d 'conf';
}

use Cwd qw(getcwd);
my $dir = getcwd;
chdir('conf') or die "chdir: $!";

open(TST,">compilerok.c") or die "open: $!";
print TST <<'ESQ';
int main () { return 0; }
ESQ
close(TST);

if (system("$Config{'cc'} $flags compilerok.c > compilerok.log 2>&1")) {
  die <<"__EOT__";

ERROR: cannot run the configured compiler '$Config{'cc'}'
(see conf/compilerok.log). Suggestions:
1) The complier '$Config{'cc'}' is not in your PATH. Add it
   to the PATH and try again. OR
2) The compiler isn't installed on your system. Install it. OR
3) You only have a different compiler installed (e.g. 'gcc').
   Either fix the compiler config in the perl Config.pm
   or install a perl that was built with the right compiler
   (you could build perl yourself with the available compiler).

Note: this is a system-administration issue, please ask your local
admin for help. Thank you.

__EOT__
}

unlink qw(compilerok.c compilerok.log);

# checking for various functions

my %funcs = (ttyname      => "",
	     openpty      => "-lutil",
	     _getpty      => "",
	     strlcpy      => "",
	     sigaction    => "",
	     grantpt      => "",
	     unlockpt     => "",
	     getpt        => "",
	     posix_openpt => "",
	     ptsname      => "",
	     ptsname_r    => "",
	    );

foreach my $f (sort keys %funcs) {
  open(TST,">functest_$f.c") or die "open: $!";
  print TST <<"ESQ";
/* System header to define __stub macros and hopefully few prototypes,
    which can conflict with char \$ac_func (); below.  */
#include <assert.h>
/* Override any gcc2 internal prototype to avoid an error.  */
#ifdef __cplusplus
extern "C"
#endif
/* We use char because int might match the return type of a gcc2
   builtin and then its argument prototype would still apply.  */
char $f ();
char (*f) ();

#ifdef F77_DUMMY_MAIN
#  ifdef __cplusplus
     extern "C"
#  endif
   int F77_DUMMY_MAIN() { return 1; }
#endif
int
main ()
{
/* The GNU C library defines this for functions which it implements
    to always fail with ENOSYS.  Some functions are actually named
    something starting with __ and the normal name is an alias.  */
#if defined (__stub_$f) || defined (__stub___$f)
choke me
#else
f = $f;
#endif

  ;
  return 0;
}
ESQ

  close(TST);
  print "Looking for $f()" . "." x (13-length($f)) . " ";
  if (system("$Config{'cc'} $flags $funcs{$f} functest_$f.c > functest_$f.log 2>&1")) {
    print "not found.\n";
  } else {
    $define{"-DHAVE_\U$f"} = undef;
    push @libs, $funcs{$f} if $funcs{$f};
    print "FOUND.\n";
    unlink "functest_$f.c", "functest_$f.log" ;
  }
}

# find various headerfiles

my @headers = qw(termios.h termio.h libutil.h util.h pty.h
                 sys/stropts.h sys/ptyio.h sys/pty.h);
my %headers;

foreach my $h (sort @headers) {
  my $def = $h;
  $def =~ s/\W/_/g;
  open(TST,">headtest_$def.c") or die "open: $!";
  print TST <<"ESQ";
#include <sys/types.h>
#include <$h>
int main () { return 0; }
ESQ
  close(TST);
  print "Looking for $h" . "." x (15-length($h)) . " ";
  if(system("$Config{'cc'} $flags headtest_$def.c > headtest_$def.log 2>&1")) {
    print "not found.\n"
  }
  else {
    $headers{$h} = undef;
    $define{"-DHAVE_\U$def"} = $h;
    print "FOUND.\n";
    unlink "headtest_$def.c", "headtest_$def.log";
  }
}

# now write xssubs

print SUB qq{sv_setpv(config, "@{[sort keys %define]}");\n};

my @ttsyms = qw(B0 B110 B115200 B1200 B134 B150 B153600 B1800 B19200
	    B200 B230400 B2400 B300 B307200 B38400 B460800 B4800 B50
	    B57600 B600 B75 B76800 B9600 BRKINT BS0 BS1 BSDLY CBAUD
	    CBAUDEXT CBRK CCTS_OFLOW CDEL CDSUSP CEOF CEOL CEOL2 CEOT
	    CERASE CESC CFLUSH CIBAUD CIBAUDEXT CINTR CKILL CLNEXT
	    CLOCAL CNSWTCH CNUL CQUIT CR0 CR1 CR2 CR3 CRDLY CREAD
	    CRPRNT CRTSCTS CRTSXOFF CRTS_IFLOW CS5 CS6 CS7 CS8 CSIZE
	    CSTART CSTOP CSTOPB CSUSP CSWTCH CWERASE DEFECHO DIOC
	    DIOCGETP DIOCSETP DOSMODE ECHO ECHOCTL ECHOE ECHOK ECHOKE
	    ECHONL ECHOPRT EXTA EXTB FF0 FF1 FFDLY FIORDCHK FLUSHO
	    HUPCL ICANON ICRNL IEXTEN IGNBRK IGNCR IGNPAR IMAXBEL
	    INLCR INPCK ISIG ISTRIP IUCLC IXANY IXOFF IXON KBENABLED
	    LDCHG LDCLOSE LDDMAP LDEMAP LDGETT LDGMAP LDIOC LDNMAP
	    LDOPEN LDSETT LDSMAP LOBLK NCCS NL0 NL1 NLDLY NOFLSH OCRNL
	    OFDEL OFILL OLCUC ONLCR ONLRET ONOCR OPOST PAGEOUT PARENB
	    PAREXT PARMRK PARODD PENDIN RCV1EN RTS_TOG TAB0 TAB1 TAB2
	    TAB3 TABDLY TCDSET TCFLSH TCGETA TCGETS TCIFLUSH TCIOFF
	    TCIOFLUSH TCION TCOFLUSH TCOOFF TCOON TCSADRAIN TCSAFLUSH
	    TCSANOW TCSBRK TCSETA TCSETAF TCSETAW TCSETCTTY TCSETS
	    TCSETSF TCSETSW TCXONC TERM_D40 TERM_D42 TERM_H45
	    TERM_NONE TERM_TEC TERM_TEX TERM_V10 TERM_V61 TIOCCBRK
	    TIOCCDTR TIOCCONS TIOCEXCL TIOCFLUSH TIOCGETD TIOCGETC
	    TIOCGETP TIOCGLTC TIOCSETC TIOCSETN TIOCSETP TIOCSLTC
	    TIOCGPGRP TIOCGSID TIOCGSOFTCAR TIOCGWINSZ TIOCHPCL
	    TIOCKBOF TIOCKBON TIOCLBIC TIOCLBIS TIOCLGET TIOCLSET
	    TIOCMBIC TIOCMBIS TIOCMGET TIOCMSET TIOCM_CAR TIOCM_CD
	    TIOCM_CTS TIOCM_DSR TIOCM_DTR TIOCM_LE TIOCM_RI TIOCM_RNG
	    TIOCM_RTS TIOCM_SR TIOCM_ST TIOCNOTTY TIOCNXCL TIOCOUTQ
	    TIOCREMOTE TIOCSBRK TIOCSCTTY TIOCSDTR TIOCSETD TIOCSIGNAL
	    TIOCSPGRP TIOCSSID TIOCSSOFTCAR TIOCSTART TIOCSTI TIOCSTOP
	    TIOCSWINSZ TM_ANL TM_CECHO TM_CINVIS TM_LCF TM_NONE TM_SET
	    TM_SNL TOSTOP VCEOF VCEOL VDISCARD VDSUSP VEOF VEOL VEOL2
	    VERASE VINTR VKILL VLNEXT VMIN VQUIT VREPRINT VSTART VSTOP
	    VSUSP VSWTCH VT0 VT1 VTDLY VTIME VWERASE WRAP XCASE XCLUDE
	    XMT1EN XTABS);

print <<_EOT_;
Checking which symbols compile OK...
(sorry for the tedious check, but some systems have not too clean
 header files, to say the least;  '+' means OK, '-' means not defined
 and '*' has compile problems...)
_EOT_

my %badsyms;
my %ttsyms_exist;

foreach my $s (sort @ttsyms) {
  $ttsyms_exist{$s} = undef;
  open(TST,">ttsymtest_$s.c") or die "open >ttsymtest_$s.c: $!";
  print TST "#include <sys/types.h>\n";
  foreach my $h (@headers) {
    print TST "#include <$h>\n" if exists $headers{$h};
  }
  print TST <<"__EOT__";
#ifdef $s
int main () { int x; x = (int)$s; return 0; }
#else
#line 29999
choke me badly on line 29999
#endif
__EOT__
  close(TST);

  if (system("$Config{'cc'} $flags @{[keys %define]} ttsymtest_$s.c >ttsymtest_$s.log 2>&1")) {
    print SUB qq{newCONSTSUB(stash, "$s", newSV(0));\n};
    # now check if the symbol is defined (should have an error message
    # for line 29999 in the logfile)
    open(CCOUT, "ttsymtest_$s.log") or die "open ttsymtest_$s.log: $!";
    if (grep {m/29999/} (<CCOUT>)) {
      # symbol not defined
      delete $ttsyms_exist{$s};
      print "-$s ";
      unlink "ttsymtest_$s.c", "ttsymtest_$s.log";
    } else {
      # was defined, but didn't compile
      $badsyms{$s} = undef;
      print "*$s ";
    }
    close CCOUT;
  }
  else {
    print "+$s ";
    print SUB qq{newCONSTSUB(stash, "$s", newSViv($s));\n};
    unlink "ttsymtest_$s.c", "ttsymtest_$s.log";
  }
}

close(SUB);
print "\n\n";

# now back to Makefile dir
chdir($dir) or die "chdir: $!";

my $all_ok = 1;
foreach my $check
  (
   {
    defines => [qw"-DHAVE_PTSNAME -DHAVE_PTSNAME_R"],
    msg => "WARNING!  Neither ptsname() nor ptsname_r() could be found,\n so we cannot use a high-level interface like openpty().\n",
   },
   {
    defines => [qw"-DHAVE_DEV_PTMX -DHAVE_DEV_PTYM_CLONE -DHAVE_DEV_PTC -DHAVE_DEV_PTMX_BSD -DHAVE__GETPTY -DHAVE_OPENPTY -DHAVE_GETPT -DHAVE_POSIX_OPENPT"],
    msg => "No high-level lib or clone device has been found, we will use BSD-style ptys.\n",
   },
  ) {
    my $any = 0;
    foreach my $x (@{$check->{defines}}) {
      $any = 1 if exists $define{$x};
    }
    if (not $any) {
      print $check->{msg};
      $all_ok = 0;
    }
  }

my %used_syms = map {($_, undef)}
  qw(TIOCSCTTY TCSETCTTY TIOCNOTTY TIOCGWINSZ TIOCSWINSZ);
foreach my $s (sort keys %badsyms) {
  if (exists $used_syms{$s}) {
    print "WARNING!  $s is used by Pty.pm but didn't compile.  This may mean reduced functionality.\n";
    $all_ok = 0;
  } else {
    print "Warning:  $s has compile problems, it's thus not available (but it's not used by Pty.pm, so that's OK).  See conf/ttsymtest_$s.log for details.\n";
  }
}

print ">>> Configuration looks good! <<<\n\n" if $all_ok;
print <<'_EOT_' if keys %badsyms;
(If you need those missing symbols, check your header files where those
are declared.  I'm expecting them to be found in either termio.h or
termios.h (and their #include hierarchy), but on some systems there
are structs required that can be found in asm/*.h or linux/*.h.  You
can try to add these to @headers and see if that helps.  Sorry, but
the fault really lies with your system vendor.)

_EOT_

print "Writing IO::Tty::Constant.pm...\n";
unless( mkdir 'Tty', 0777 ) {
  my $e = $!;
  die "mkdir: $e" unless -d 'Tty';
}
open (POD, ">Tty/Constant.pm") or die "open: $!";
print POD <<"_EOT_";

package IO::Tty::Constant;

use vars qw(\@ISA \@EXPORT_OK);
require Exporter;

\@ISA = qw(Exporter);
\@EXPORT_OK = qw(@ttsyms);

__END__

=head1 NAME

IO::Tty::Constant - Terminal Constants (autogenerated)

=head1 SYNOPSIS

 use IO::Tty::Constant qw(TIOCNOTTY);
 ...

=head1 DESCRIPTION

This package defines constants usually found in <termio.h> or
<termios.h> (and their #include hierarchy).  Find below an
autogenerated alphabetic list of all known constants and whether they
are defined on your system (prefixed with '+') and have compilation
problems ('o').  Undefined or problematic constants are set to 'undef'.

=head1 DEFINED CONSTANTS

_EOT_

foreach my $s (@ttsyms) {
  if (exists $badsyms{$s}) {
    print POD "=item *\n\n";
  } elsif (exists $ttsyms_exist{$s}) {
    print POD "=item +\n\n";
  } else {
    print POD "=item -\n\n";
  }
  print POD "$s\n\n";
}

print POD <<_EOT_;

=head1 FOR MORE INFO SEE

L<IO::Tty>

=cut

_EOT_

close POD;

print <<'__EOT__' if $Is_Beta;
**********************************************************************
WARNING: this is a BETA version.  If it works, good for you, if not,
tell me, <RGiersig@cpan.org> about it (including full output of
'perl Makefile.PL; make; make test;') and I'll see what I can do.
**********************************************************************
__EOT__

print "DEFINE = @{[sort keys %define]}\n";

WriteMakefile(
    'NAME'	    => 'IO::Tty',
    'VERSION'       => $Package_Version,
    'DEFINE'	    => join(" ", sort keys %define),
    'LIBS'          => join(" ", @libs),
    'clean'	    => {'FILES' => 'xssubs.c conf Tty.exp_old log'},
    'realclean'	    => {'FILES' => 'Tty IO-Tty.ppd'},
    'MAP_TARGET'    => 'perltty',
    ($] >= 5.00503) ?
    ( AUTHOR    => 'Roland Giersig <RGiersig@cpan.org>',
      ABSTRACT  => 'Pseudo ttys and constants' ) :
    (),
);

sub MY::postamble {

  return '' unless $] >= 5.00503;

<<'ESQ';

dist : ppd

ESQ
}

