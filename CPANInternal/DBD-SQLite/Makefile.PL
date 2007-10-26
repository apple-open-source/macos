use ExtUtils::MakeMaker;
eval {
    require DBI;
    require DBI::DBD;
    die "Your DBI Version is too old - DBD::SQLite requires at least 1.21"
        unless $DBI::VERSION >= 1.21;
};
if ($@) {
    warn $@;
}
use Config;
use strict;
eval {
    require DBD::SQLite;
    if ($DBD::SQLite::VERSION < 1.0) {
        print <<EOT;

**** WARNING **** WARNING **** WARNING **** WARNING **** WARNING ****

Your version of DBD::SQLite currently installed ($DBD::SQLite::VERSION) uses
the old sqlite database format. This version of DBD::SQLite will *NOT*
open these files, and installing this module may cause problems on your
system. If this is a live environment you should upgrade with caution.

To upgrade a database, download and install both sqlite 2.x and 3.x from
http://www.sqlite.org/ and issue:

  sqlite OLD.DB .dump | sqlite3 NEW.DB

DBD::SQLite will NOT automatically upgrade a database for you, and using
this version against an old SQLite database WILL lead to database
corruption.

EOT
        if (prompt("Continue?", "N") !~ /^y/i) {
            print "Exiting\n";
            exit -1;
        }
    }
};

# 2005/6/19, by rjray@blackperl.com
#
# Determine if we are going to use the provided SQLite code, or an already-
# installed copy. To this end, look for two command-line parameters:
#
#    USE_LOCAL_SQLITE -- If non-false, force use of the installed version
#    SQLITE_LOCATION  -- If passed, look for headers and libs under this root
#
# In absense of either of those, expect SQLite 3.X.X libs and headers in the
# common places known to Perl or the C compiler.
require File::Spec;
my ($force_local, $sqlite_base, $sqlite_lib, $sqlite_inc);
if ($sqlite_base = (grep(/SQLITE_LOCATION=.*/, @ARGV))[0]) {
    $sqlite_base =~ /=(.*)/;
    $sqlite_base = $1;
    $sqlite_lib = File::Spec->catdir($sqlite_base, 'lib');
    $sqlite_inc = File::Spec->catdir($sqlite_base, 'include');
}
if ($force_local = (grep(/USE_LOCAL_SQLITE=.*/, @ARGV))[0]) {
    $force_local =~ /=(.*)/;
    $force_local = "$1" ? 1 : 0;
    if ($force_local) {
        undef $sqlite_lib; # Keep these from making into CFLAGS/LDFLAGS
        undef $sqlite_inc;
    }
}

# Now, check for a compatible sqlite3
unless ($force_local) {
    my ($dir, $file, $fh, $version);
    print "Checking installed SQLite version...\n";
    if ($sqlite_inc) {
        open($fh, '< ' . File::Spec->catfile($sqlite_inc, 'sqlite3.h'))
            or die "Error opening sqlite3.h in $sqlite_inc: $!";
        while (defined($_ = <$fh>)) {
            if (/\#define\s+SQLITE_VERSION_NUMBER\s+(\d+)/) {
                $version = $1;
                last;
            }
        }
        close($fh);
    } else {
        # Go hunting for the file (Matt: Add more dirs here as you see fit)
        for $dir ([ qw(usr include) ], [ qw(usr local include) ]) {
            $file = File::Spec->catfile('', @$dir, 'sqlite3.h');
            next unless (-f $file);
            open($fh, "< $file") or die "Error opening $file: $!";
            while (defined($_ = <$fh>)) {
                if (/\#define\s+SQLITE_VERSION_NUMBER\s+(\d+)/) {
                    $version = $1;
                    last;
                }
            }
            close($fh);
            last if $version;
        }
    }
    unless ($version && ($version >= 3001003)) {
        warn "SQLite version must be at least 3.1.3. No header file at that\n";
        warn "version or higher was found. Using the local version instead.\n";
        $force_local = 1;
        undef $sqlite_lib;
        undef $sqlite_inc;
    } else {
        print "Looks good\n";
    }
}

@ARGV = grep(! /SQLITE_LOCATION|USE_LOCAL_SQLITE/, @ARGV);

my $nlid = $DBI::VERSION > 1.42 ? '' : '-Dno_last_insert_id';

my $libs = '';
$libs .= "-L$sqlite_lib " if ($sqlite_lib); 
$libs .= "-lsqlite3 " unless ($force_local);

WriteMakefile(
    'NAME'          => 'DBD::SQLite',
    'VERSION_FROM'  => 'lib/DBD/SQLite.pm', # finds $VERSION
    'PREREQ_PM'     => {DBI => 1.21}, # e.g., Module::Name => 1.1
    'OBJECT'        => ($force_local) ? '$(O_FILES)' : 'SQLite.o dbdimp.o',
    'INC'           => '-I. -I$(DBI_INSTARCH_DIR)' .
                       (($sqlite_inc) ? " -I$sqlite_inc" : ''),
    $libs ? ('LIBS' => $libs) : (),
    'OPTIMIZE'      => "-O2",
    'DEFINE'        => "-DNDEBUG=1 -DSQLITE_PTR_SZ=$Config{ptrsize}" .
                       ( ($Config{d_usleep} ||
                          $Config{osname} =~ m/linux/) ?
                                " -DHAVE_USLEEP=1" : "" ) .
                       ($DBI::VERSION > 1.42 ? '' : ' -Dno_last_insert_id'),
    'clean'         => { FILES => 'SQLite.xsi config.h tv.log output' },
    'PL_FILES'      => {},
    'EXE_FILES'     => [],
);

package MY;
sub postamble { 
  eval { DBI::DBD::dbd_postamble(@_) };
}
sub libscan {
    my ($self, $path) = @_;
    return if $path =~ /\.pl$/;
    ($path =~ m/\~$/) ? undef : $path;
}
