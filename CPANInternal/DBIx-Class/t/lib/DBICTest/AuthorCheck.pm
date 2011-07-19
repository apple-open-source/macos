package # hide from PAUSE 
    DBICTest::AuthorCheck;

use strict;
use warnings;

use Path::Class qw/file dir/;

_check_author_makefile() unless $ENV{DBICTEST_NO_MAKEFILE_VERIFICATION};

# Die if the author did not update his makefile
#
# This is pretty heavy handed, so the check is pretty solid:
#
# 1) Assume that this particular module is loaded from -I <$root>/t/lib
# 2) Make sure <$root>/Makefile.PL exists
# 3) Make sure we can stat() <$root>/Makefile.PL
#
# If all of the above is satisfied
#
# *) die if <$root>/inc does not exist
# *) die if no stat() results for <$root>/Makefile (covers no Makefile)
# *) die if Makefile.PL mtime > Makefile mtime
#
sub _check_author_makefile {

  my $root = _find_co_root()
    or return;

  my $optdeps = file('lib/DBIx/Class/Optional/Dependencies.pm');

  # not using file->stat as it invokes File::stat which in turn breaks stat(_)
  my ($mf_pl_mtime, $mf_mtime, $optdeps_mtime) = ( map
    { (stat ($root->file ($_)) )[9] }
    (qw|Makefile.PL  Makefile|, $optdeps)
  );

  return unless $mf_pl_mtime;   # something went wrong during co_root detection ?

  my @fail_reasons;

  if(not -d $root->subdir ('inc')) {
    push @fail_reasons, "Missing ./inc directory";
  }

  if (not $mf_mtime) {
    push @fail_reasons, "Missing ./Makefile";
  }
  elsif($mf_mtime < $mf_pl_mtime) {
    push @fail_reasons, "./Makefile.PL is newer than ./Makefile";
  }

  if ($mf_mtime < $optdeps_mtime) {
    push @fail_reasons, "./$optdeps is newer than ./Makefile";
  }

  if (@fail_reasons) {
    print STDERR <<'EOE';


!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
======================== FATAL ERROR ===========================
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

We have a number of reasons to believe that this is a development
checkout and that you, the user, did not run `perl Makefile.PL`
before using this code. You absolutely _must_ perform this step,
and ensure you have all required dependencies present. Not doing
so often results in a lot of wasted time for other contributors
trying to assit you with spurious "its broken!" problems.

If you are seeing this message unexpectedly (i.e. you are in fact
attempting a regular installation be it through CPAN or manually),
please report the situation to either the mailing list or to the
irc channel as described in

http://search.cpan.org/dist/DBIx-Class/lib/DBIx/Class.pm#GETTING_HELP/SUPPORT

The DBIC team


Reasons you received this message:

EOE

    foreach my $r (@fail_reasons) {
      print STDERR "  * $r\n";
    }
    print STDERR "\n\n\n";

    exit 1;
  }
}

# Mimic $Module::Install::AUTHOR
sub is_author {

  my $root = _find_co_root()
    or return undef;

  return (
    ( not -d $root->subdir ('inc') )
      or
    ( -e $root->subdir ('inc')->file ($^O eq 'VMS' ? '_author' : '.author') )
  );
}

# Try to determine the root of a checkout/untar if possible
# or return undef
sub _find_co_root {

    my @mod_parts = split /::/, (__PACKAGE__ . '.pm');
    my $rel_path = join ('/', @mod_parts);  # %INC stores paths with / regardless of OS

    return undef unless ($INC{$rel_path});

    # a bit convoluted, but what we do here essentially is:
    #  - get the file name of this particular module
    #  - do 'cd ..' as many times as necessary to get to t/lib/../..

    my $root = dir ($INC{$rel_path});
    for (1 .. @mod_parts + 2) {
        $root = $root->parent;
    }

    return (-f $root->file ('Makefile.PL') )
      ? $root
      : undef
    ;
}

1;
