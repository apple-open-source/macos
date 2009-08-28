use ExtUtils::MakeMaker;
use Config;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

#added by Lincoln Baxter to fix cflags (for long long on HPUX)
#guidence from DBD-Oracle module
{
    package MY; # SUPER needs package context, $self is not sufficient
    use strict;
    use Config;
    my $os = $^O;

    sub const_cccmd {
        my ($self) = shift;
        local($_) = $self->SUPER::const_cccmd(@_);
        # are we using the non-bundled hpux compiler?
        if ($os eq "hpux" and $Config::Config{ccflags} =~ /-Aa\b/) {
            print "Changing -Aa to -Ae for HP-UX in ccmd to allow for long long.\n"
             if s/-Aa\b/-Ae/g;  # allow "long long" in UUID.h
        }
        $_;
    }
    sub cflags 
    {
        my ($self) = shift;
        local($_) = $self->SUPER::cflags(@_);
        # are we using the non-bundled hpux compiler?
        if ($os eq "hpux" and $Config::Config{ccflags} =~ /-Aa\b/) {
            print "Changing -Aa to -Ae for HP-UX in cflags.\n"
             if s/-Aa\b/-Ae/g;  # allow "long long" in UUID.h
        }
        $_;
    }
};

WriteMakefile(
  ($] >= 5.005 ## Add these new keywords supported since 5.005
  ? (ABSTRACT_FROM => 'UUID.pm', # retrieve abstract from module
     AUTHOR        => 'Ricardo Signes <rjbs[at]cpan.org>')
  : ()),

  NAME                => 'Data::UUID',
  VERSION_FROM        => 'UUID.pm', # finds $VERSION
  PREREQ_PM           => {}, # e.g., Module::Name => 1.1
  LICENSE             => 'unknown',
  LIBS                => [], # e.g., '-lm'
  #works without -lsocket
  DEFINE              => '', # e.g., '-DHAVE_SOMETHING'
  # Insert -I. if you add *.h files later:
  INC                 => '', # e.g., '-I/usr/include/other'
  # Un-comment this if you add C files to link with later:
  OBJECT              => '$(O_FILES)', # link all the C files too
  CONFIGURE           => sub {
    my $d;
    if ($^O eq 'MSWin32' and -d "c:/tmp/") {
     $d="c:/tmp";
    } else {
     $d=eval { require File::Spec; File::Spec->tmpdir; } || '/var/tmp';
    }
    $d = prompt("UUID state storage", $d);

    my $m = '0007';
    $m = prompt("default umask", $m) unless $^O eq 'MSWin32';

    chmod(0666, sprintf("%s/%s", $d, ".UUID_NODEID"));
    chmod(0666, sprintf("%s/%s", $d, ".UUID_STATE"));
    return {
      DEFINE => qq(-D_STDIR=\\"$d\\")
              . qq( -D__$Config{osname}__)
              . qq( -D_DEFAULT_UMASK=$m)
    };
  }
);
