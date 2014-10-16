use 5.006;
use strict;
use warnings;
use ExtUtils::MakeMaker;
use Config;
use Getopt::Long qw(GetOptions :config pass_through);
use Pod::Usage qw(pod2usage);
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
  PREREQ_PM           => { 'Digest::MD5' => '0' }, # e.g., Module::Name => 1.1
  LICENSE             => 'bsd',
  LIBS                => [], # e.g., '-lm'
  #works without -lsocket
  DEFINE              => '', # e.g., '-DHAVE_SOMETHING'
  # Insert -I. if you add *.h files later:
  INC                 => '', # e.g., '-I/usr/include/other'
  # Un-comment this if you add C files to link with later:
  OBJECT              => '$(O_FILES)', # link all the C files too
  META_MERGE => {
    resources => {
      bugtracker  => 'https://github.com/rjbs/Data-UUID/issues',
      repository  => 'https://github.com/rjbs/Data-UUID',
    },
},

  CONFIGURE           => sub {
    my %opt;
    GetOptions(\%opt, 's|state-storage-directory:s', 'd|default-umask:s',
        'help|?', 'man') or pod2usage(2);
    pod2usage(1) if $opt{help};
    pod2usage(-verbose => 2) if $opt{man};

    print "Configured options (run perl Makefile.PL --help for how to change this):\n";

    my $d;
    if ($^O eq 'MSWin32' and -d "c:/tmp/") {
     $d="c:/tmp";
    } else {
     $d=eval { require File::Spec; File::Spec->tmpdir; } || '/var/tmp';
    }
    $d = $opt{s} || $d;
    print "\tUUID state storage: $d\n";
    $d =~ s/\\/\\\\/g if $^O eq 'MSWin32';

    my $m = '0007';
    unless ($^O eq 'MSWin32') {
        $m = $opt{d} || $m;
        print "\tdefault umask: $m\n";
    }

    chmod(0666, sprintf("%s/%s", $d, ".UUID_NODEID"));
    chmod(0666, sprintf("%s/%s", $d, ".UUID_STATE"));
    return {
      DEFINE => qq(-D_STDIR=\\"$d\\")
              . qq( -D__$Config{osname}__)
              . qq( -D_DEFAULT_UMASK=$m)
    };
  }
);

__END__

=head1 NAME

Makefile.PL - configure Makefile for Data::UUID

=head1 SYNOPSIS

perl Makefile.PL [options] [EU::MM options]

perl Makefile.PL -s=/var/local/lib/data-uuid -d=0007

    Options:
    --state-storage-directory   directory for storing library state information
    --default-umask             umask for files in the state storage directory
    --help                      brief help message
    --man                       full documentation

Options can be abbreviated, see L<Getopt::Long/"Case and abbreviations">.

=head1 OPTIONS

=over

=item --state-storage-directory

Optional. Takes a string that is interpreted as directory for storing library
state information. Default is c:/tmp/ on Windows if it already exists, or the
operating system's temporary directory (see tmpdir in L<File::Spec/"METHODS">),
or /var/tmp as fallback.

=item --default-umask

Optional. Takes a string that is interpreted as umask for the files in the state
storage directory. Default is 0007. This is ignored on Windows.

=item --help

Print a brief help message and exits.

=item --man

Prints the manual page and exits.

=back

=head1 DESCRIPTION

B<Makefile.PL> writes the Makefile for the Data::UUID library. It is configured
with the options L</"--state-storage-directory"> and L</"--default-umask">.
Unless given, default values are used. In any case the values are printed for
confirmation.

Additionally, the usual EU::MM options are processed, see
L<ExtUtils::MakeMaker/"Using Attributes and Parameters">.
