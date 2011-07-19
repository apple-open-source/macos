# -*- perl -*-
#
#   DBD::File - A base class for implementing DBI drivers that
#               act on plain files
#
#  This module is currently maintained by
#
#      H.Merijn Brand & Jens Rehsack
#
#  The original author is Jochen Wiedmann.
#
#  Copyright (C) 2009 by H.Merijn Brand & Jens Rehsack
#  Copyright (C) 2004 by Jeff Zucker
#  Copyright (C) 1998 by Jochen Wiedmann
#
#  All rights reserved.
#
#  You may distribute this module under the terms of either the GNU
#  General Public License or the Artistic License, as specified in
#  the Perl README file.

require 5.005;

use strict;

use DBI ();
require DBI::SQL::Nano;
require File::Spec;

package DBD::File;

use strict;

use vars qw( @ISA $VERSION $drh $valid_attrs );

$VERSION = "0.37";

$drh = undef;		# holds driver handle(s) once initialised

sub driver ($;$)
{
    my ($class, $attr) = @_;

    # Drivers typically use a singleton object for the $drh
    # We use a hash here to have one singleton per subclass.
    # (Otherwise DBD::CSV and DBD::DBM, for example, would
    # share the same driver object which would cause problems.)
    # An alternative would be not not cache the $drh here at all
    # and require that subclasses do that. Subclasses should do
    # their own caching, so caching here just provides extra safety.
    $drh->{$class} and return $drh->{$class};

    DBI->setup_driver ("DBD::File"); # only needed once but harmless to repeat
    $attr ||= {};
    {	no strict "refs";
	unless ($attr->{Attribution}) {
	    $class eq "DBD::File" and $attr->{Attribution} = "$class by Jeff Zucker";
	    $attr->{Attribution} ||= ${$class . "::ATTRIBUTION"} ||
		"oops the author of $class forgot to define this";
	    }
	$attr->{Version} ||= ${$class . "::VERSION"};
	$attr->{Name} or ($attr->{Name} = $class) =~ s/^DBD\:\://;
	}

    $drh->{$class} = DBI::_new_drh ($class . "::dr", $attr);
    $drh->{$class}->STORE (ShowErrorStatement => 1);
    return $drh->{$class};
    } # driver

sub CLONE
{
    undef $drh;
    } # CLONE

sub file2table
{
    my ($data, $dir, $file, $file_is_tab, $quoted) = @_;

    $file eq "." || $file eq ".."	and return;

    my ($ext, $req) = ("", 0, 0);
    if ($data->{f_ext}) {
	($ext, my $opt) = split m/\//, $data->{f_ext};
	if ($ext && $opt) {
	    $opt =~ m/r/i and $req = 1;
	    }
	}

    (my $tbl = $file) =~ s/$ext$//i;
    $file_is_tab and $file = "$tbl$ext";

    # Fully Qualified File Name
    my $fqfn;
    unless ($quoted) { # table names are case insensitive in SQL
	local *DIR;
	opendir DIR, $dir;
	my @f = grep { lc $_ eq lc $file } readdir DIR;
	@f == 1 and $file = $f[0];
	}
    $fqfn = File::Spec->catfile ($dir, $file);

    $file = $fqfn;
    if ($ext) {
	if ($req) {
	    # File extension required
	    $file =~ s/$ext$//i			or  return;
	    }
	else {
	    # File extension optional, skip if file with extension exists
	    grep m/$ext$/i, glob "$fqfn.*"	and return;
	    $file =~ s/$ext$//i;
	    }
	}

    $data->{f_map}{$tbl} = $fqfn;
    return $tbl;
    } # file2table

# ====== DRIVER ================================================================

package DBD::File::dr;

use strict;

$DBD::File::dr::imp_data_size = 0;

sub connect ($$;$$$)
{
    my ($drh, $dbname, $user, $auth, $attr)= @_;

    # create a 'blank' dbh
    my $this = DBI::_new_dbh ($drh, {
	Name		=> $dbname,
	USER		=> $user,
	CURRENT_USER	=> $user,
	});

    if ($this) {
	my ($var, $val);
	$this->{f_dir} = File::Spec->curdir ();
	$this->{f_ext} = "";
	$this->{f_map} = {};
	while (length $dbname) {
	    if ($dbname =~ s/^((?:[^\\;]|\\.)*?);//s) {
		$var    = $1;
		}
	    else {
		$var    = $dbname;
		$dbname = "";
		}
	    if ($var =~ m/^(.+?)=(.*)/s) {
		$var = $1;
		($val = $2) =~ s/\\(.)/$1/g;
		$this->{$var} = $val;
		}
	    }
        $this->{f_valid_attrs} = {
	    f_version	=> 1, # DBD::File version
	    f_dir	=> 1, # base directory
	    f_ext	=> 1, # file extension
	    f_schema	=> 1, # schema name
	    f_tables	=> 1, # base directory
	    };
        $this->{sql_valid_attrs} = {
	    sql_handler           => 1, # Nano or S:S
	    sql_nano_version      => 1, # Nano version
	    sql_statement_version => 1, # S:S version
	    };
	}
    $this->STORE ("Active", 1);
    return set_versions ($this);
    } # connect

sub set_versions
{
    my $this = shift;
    $this->{f_version} = $DBD::File::VERSION;
    for (qw( nano_version statement_version)) {
	$this->{"sql_$_"} = $DBI::SQL::Nano::versions->{$_} || "";
	}
    $this->{sql_handler} = $this->{sql_statement_version}
	? "SQL::Statement"
	: "DBI::SQL::Nano";
    return $this;
    } # set_versions

sub data_sources ($;$)
{
    my ($drh, $attr) = @_;
    my $dir = $attr && exists $attr->{f_dir}
	? $attr->{f_dir}
	: File::Spec->curdir ();
    my ($dirh) = Symbol::gensym ();
    unless (opendir ($dirh, $dir)) {
	$drh->set_err ($DBI::stderr, "Cannot open directory $dir: $!");
	return;
	}

    my ($file, @dsns, %names, $driver);
    if ($drh->{ImplementorClass} =~ m/^dbd\:\:([^\:]+)\:\:/i) {
	$driver = $1;
	}
    else {
	$driver = "File";
	}

    while (defined ($file = readdir ($dirh))) {
	if ($^O eq "VMS") {
	    # if on VMS then avoid warnings from catdir if you use a file
	    # (not a dir) as the file below
	    $file !~ m/\.dir$/oi and next;
	    }
	my $d = File::Spec->catdir ($dir, $file);
	# allow current dir ... it can be a data_source too
	$file ne File::Spec->updir () && -d $d and
	    push @dsns, "DBI:$driver:f_dir=$d";
	}
    @dsns;
    } # data_sources

sub disconnect_all
{
    } # disconnect_all

sub DESTROY
{
    undef;
    } # DESTROY

# ====== DATABASE ==============================================================

package DBD::File::db;

use strict;
use Carp;

$DBD::File::db::imp_data_size = 0;

sub ping
{
    return (shift->FETCH ("Active")) ? 1 : 0;
    } # ping

sub prepare ($$;@)
{
    my ($dbh, $statement, @attribs) = @_;

    # create a 'blank' sth
    my $sth = DBI::_new_sth ($dbh, {Statement => $statement});

    if ($sth) {
	my $class = $sth->FETCH ("ImplementorClass");
	$class =~ s/::st$/::Statement/;
	my $stmt;

	# if using SQL::Statement version > 1
	# cache the parser object if the DBD supports parser caching
	# SQL::Nano and older SQL::Statements don't support this

	if ( $dbh->{sql_handler} eq "SQL::Statement" and
	     $dbh->{sql_statement_version} > 1) {
	    my $parser = $dbh->{csv_sql_parser_object};
	    $parser ||= eval { $dbh->func ("csv_cache_sql_parser_object") };
	    if ($@) {
		$stmt = eval { $class->new ($statement) };
		}
	    else {
		$stmt = eval { $class->new ($statement, $parser) };
		}
	    }
	else {
	    $stmt = eval { $class->new ($statement) };
	    }
	if ($@) {
	    $dbh->set_err ($DBI::stderr, $@);
	    undef $sth;
	    }
	else {
	    $sth->STORE ("f_stmt", $stmt);
	    $sth->STORE ("f_params", []);
	    $sth->STORE ("NUM_OF_PARAMS", scalar ($stmt->params ()));
	    }
	}
    $sth;
    } # prepare

sub csv_cache_sql_parser_object
{
    my $dbh    = shift;
    my $parser = {
	dialect    => "CSV",
	RaiseError => $dbh->FETCH ("RaiseError"),
	PrintError => $dbh->FETCH ("PrintError"),
	};
    my $sql_flags = $dbh->FETCH ("sql_flags") || {};
    %$parser = (%$parser, %$sql_flags);
    $parser = SQL::Parser->new ($parser->{dialect}, $parser);
    $dbh->{csv_sql_parser_object} = $parser;
    return $parser;
    } # csv_cache_sql_parser_object

sub disconnect ($)
{
    shift->STORE ("Active", 0);
    1;
    } # disconnect

sub FETCH ($$)
{
    my ($dbh, $attrib) = @_;
    $attrib eq "AutoCommit" and
	return 1;

    if ($attrib eq (lc $attrib)) {
	# Driver private attributes are lower cased

	# Error-check for valid attributes
	# not implemented yet, see STORE
	#
	return $dbh->{$attrib};
	}
    # else pass up to DBI to handle
    return $dbh->SUPER::FETCH ($attrib);
    } # FETCH

sub STORE ($$$)
{
    my ($dbh, $attrib, $value) = @_;

    if ($attrib eq "AutoCommit") {
	$value and return 1;    # is already set
	croak "Can't disable AutoCommit";
	}

    if ($attrib eq lc $attrib) {
	# Driver private attributes are lower cased

	# I'm not implementing this yet becuase other drivers may be
	# setting f_ and sql_ attrs I don't know about
	# I'll investigate and publicize warnings to DBD authors
	# then implement this

	# return to implementor if not f_ or sql_
	# not implemented yet
	# my $class = $dbh->FETCH ("ImplementorClass");
	#
	# !$dbh->{f_valid_attrs}->{$attrib} && !$dbh->{sql_valid_attrs}->{$attrib} and
	#    return $dbh->set_err ($DBI::stderr, "Invalid attribute '$attrib'");
	#  $dbh->{$attrib} = $value;

	if ($attrib eq "f_dir") {
	    -d $value or
		return $dbh->set_err ($DBI::stderr, "No such directory '$value'")
	    }
	if ($attrib eq "f_ext") {
	    $value eq "" || $value =~ m{^\.\w+(?:/[rR]*)?$}
		or carp "'$value' doesn't look like a valid file extension attribute\n";
	    }
	$dbh->{$attrib} = $value;
	return 1;
	}
    return $dbh->SUPER::STORE ($attrib, $value);
    } # STORE

sub DESTROY ($)
{
    my $dbh = shift;
    $dbh->SUPER::FETCH ("Active") and $dbh->disconnect ;
    } # DESTROY

sub type_info_all ($)
{
    [ { TYPE_NAME          => 0,
	DATA_TYPE          => 1,
	PRECISION          => 2,
	LITERAL_PREFIX     => 3,
	LITERAL_SUFFIX     => 4,
	CREATE_PARAMS      => 5,
	NULLABLE           => 6,
	CASE_SENSITIVE     => 7,
	SEARCHABLE         => 8,
	UNSIGNED_ATTRIBUTE => 9,
	MONEY              => 10,
	AUTO_INCREMENT     => 11,
	LOCAL_TYPE_NAME    => 12,
	MINIMUM_SCALE      => 13,
	MAXIMUM_SCALE      => 14,
	},
      [ "VARCHAR",	DBI::SQL_VARCHAR (),
	undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999,
	],
      [ "CHAR",		DBI::SQL_CHAR (),
	undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999,
	],
      [ "INTEGER",	DBI::SQL_INTEGER (),
	undef, "",  "",  undef, 0, 0, 1, 0, 0, 0, undef, 0, 0,
	],
      [ "REAL",		DBI::SQL_REAL (),
	undef, "",  "",  undef, 0, 0, 1, 0, 0, 0, undef, 0, 0,
	],
      [ "BLOB",		DBI::SQL_LONGVARBINARY (),
	undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999,
	],
      [ "BLOB",		DBI::SQL_LONGVARBINARY (),
	undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999,
	],
      [ "TEXT",		DBI::SQL_LONGVARCHAR (),
	undef, "'", "'", undef, 0, 1, 1, 0, 0, 0, undef, 1, 999999,
	]];
    } # type_info_all

{   my $names = [
	qw( TABLE_QUALIFIER TABLE_OWNER TABLE_NAME TABLE_TYPE REMARKS )];

    sub table_info ($)
    {
	my $dbh  = shift;
	my $dir  = $dbh->{f_dir};
	my $dirh = Symbol::gensym ();

	unless (opendir $dirh, $dir) {
	    $dbh->set_err ($DBI::stderr, "Cannot open directory $dir: $!");
	    return;
	    }

	my ($file, @tables, %names);
	my $schema = exists $dbh->{f_schema}
	    ? $dbh->{f_schema}
	    : eval { getpwuid ((stat $dir)[4]) };
	while (defined ($file = readdir ($dirh))) {
	    my $tbl = DBD::File::file2table ($dbh, $dir, $file, 0, 0) or next;
	    push @tables, [ undef, $schema, $tbl, "TABLE", undef ];
	    }
	unless (closedir $dirh) {
	    $dbh->set_err ($DBI::stderr, "Cannot close directory $dir: $!");
	    return;
	    }

	my $dbh2 = $dbh->{csv_sponge_driver};
	unless ($dbh2) {
	    $dbh2 = $dbh->{csv_sponge_driver} = DBI->connect ("DBI:Sponge:");
	    unless ($dbh2) {
		$dbh->set_err ($DBI::stderr, $DBI::errstr);
		return;
		}
	    }

	# Temporary kludge: DBD::Sponge dies if @tables is empty. :-(
	@tables or return;

	my $sth = $dbh2->prepare ("TABLE_INFO", {
				    rows  => \@tables,
				    NAMES => $names,
				    });
	$sth or $dbh->set_err ($DBI::stderr, $dbh2->errstr);
	$sth;
	} # table_info
    }

sub list_tables ($)
{
    my $dbh = shift;
    my ($sth, @tables);
    $sth = $dbh->table_info () or return;
    while (my $ref = $sth->fetchrow_arrayref ()) {
	push @tables, $ref->[2];
	}
    @tables;
    } # list_tables

sub quote ($$;$)
{
    my ($self, $str, $type) = @_;
    defined $str or return "NULL";
    defined $type && (
	    $type == DBI::SQL_NUMERIC  ()
	 || $type == DBI::SQL_DECIMAL  ()
	 || $type == DBI::SQL_INTEGER  ()
	 || $type == DBI::SQL_SMALLINT ()
	 || $type == DBI::SQL_FLOAT    ()
	 || $type == DBI::SQL_REAL     ()
	 || $type == DBI::SQL_DOUBLE   ()
	 || $type == DBI::SQL_TINYINT  ())
	and return $str;

    $str =~ s/\\/\\\\/sg;
    $str =~ s/\0/\\0/sg;
    $str =~ s/\'/\\\'/sg;
    $str =~ s/\n/\\n/sg;
    $str =~ s/\r/\\r/sg;
    "'$str'";
    } # quote

sub commit ($)
{
    my $dbh = shift;
    $dbh->FETCH ("Warn") and
	carp "Commit ineffective while AutoCommit is on", -1;
    1;
    } # commit

sub rollback ($)
{
    my $dbh = shift;
    $dbh->FETCH ("Warn") and
	carp "Rollback ineffective while AutoCommit is on", -1;
    0;
    } # rollback

# ====== STATEMENT =============================================================

package DBD::File::st;

use strict;

$DBD::File::st::imp_data_size = 0;

sub bind_param ($$$;$)
{
    my ($sth, $pNum, $val, $attr) = @_;
    if ($attr && defined $val) {
	my $type = ref $attr eq "HASH" ? $attr->{TYPE} : $attr;
	if (   $attr == DBI::SQL_BIGINT ()
	    || $attr == DBI::SQL_INTEGER ()
	    || $attr == DBI::SQL_SMALLINT ()
	    || $attr == DBI::SQL_TINYINT ()
	    ) {
	    $val += 0;
	    }
	elsif ($attr == DBI::SQL_DECIMAL ()
	    || $attr == DBI::SQL_DOUBLE ()
	    || $attr == DBI::SQL_FLOAT ()
	    || $attr == DBI::SQL_NUMERIC ()
	    || $attr == DBI::SQL_REAL ()
	    ) {
	    $val += 0.;
	    }
	else {
	    $val = "$val";
	    }
	}
    $sth->{f_params}[$pNum - 1] = $val;
    1;
    } # bind_param

sub execute
{
    my $sth = shift;
    my $params = @_ ? ($sth->{f_params} = [ @_ ]) : $sth->{f_params};

    $sth->finish;
    my $stmt = $sth->{f_stmt};
    unless ($sth->{f_params_checked}++) {
	# bug in SQL::Statement 1.20 and below causes breakage
	# on all but the first call
	unless ((my $req_prm = $stmt->params ()) == (my $nparm = @$params)) {
	    my $msg = "You passed $nparm parameters where $req_prm required";
	    $sth->set_err ($DBI::stderr, $msg);
	    return;
	    }
	}
    my @err;
    my $result = eval {
	local $SIG{__WARN__} = sub { push @err, @_ };
	$stmt->execute ($sth, $params);
	};
    if ($@ || @err) {
	$sth->set_err ($DBI::stderr, $@ || $err[0]);
	return undef;
	}

    if ($stmt->{NUM_OF_FIELDS}) {    # is a SELECT statement
	$sth->STORE (Active => 1);
	$sth->FETCH ("NUM_OF_FIELDS") or
	    $sth->STORE ("NUM_OF_FIELDS", $stmt->{NUM_OF_FIELDS})
	}
    return $result;
    } # execute

sub finish
{
    my $sth = shift;
    $sth->SUPER::STORE (Active => 0);
    delete $sth->{f_stmt}->{data};
    return 1;
    } # finish

sub fetch ($)
{
    my $sth  = shift;
    my $data = $sth->{f_stmt}->{data};
    if (!$data || ref $data ne "ARRAY") {
	$sth->set_err ($DBI::stderr,
	    "Attempt to fetch row without a preceeding execute () call or from a non-SELECT statement"
	    );
	return
	}
    my $dav = shift @$data;
    unless ($dav) {
	$sth->finish;
	return
	}
    if ($sth->FETCH ("ChopBlanks")) {
	$_ && $_ =~ s/\s+$// for @$dav;
	}
    $sth->_set_fbav ($dav);
    } # fetch
*fetchrow_arrayref = \&fetch;

my %unsupported_attrib = map { $_ => 1 } qw( TYPE PRECISION );

sub FETCH ($$)
{
    my ($sth, $attrib) = @_;
    exists $unsupported_attrib{$attrib}
	and return undef;    # Workaround for a bug in DBI 0.93
    $attrib eq "NAME" and
	return $sth->FETCH ("f_stmt")->{NAME};
    if ($attrib eq "NULLABLE") {
	my ($meta) = $sth->FETCH ("f_stmt")->{NAME};    # Intentional !
	$meta or return undef;
	return [ (1) x @$meta ];
	}
    if ($attrib eq lc $attrib) {
	# Private driver attributes are lower cased
	return $sth->{$attrib};
	}
    # else pass up to DBI to handle
    return $sth->SUPER::FETCH ($attrib);
    } # FETCH

sub STORE ($$$)
{
    my ($sth, $attrib, $value) = @_;
    exists $unsupported_attrib{$attrib}
	and return;    # Workaround for a bug in DBI 0.93
    if ($attrib eq lc $attrib) {
	# Private driver attributes are lower cased
	$sth->{$attrib} = $value;
	return 1;
	}
    return $sth->SUPER::STORE ($attrib, $value);
    } # STORE

sub DESTROY ($)
{
    my $sth = shift;
    $sth->SUPER::FETCH ("Active") and $sth->finish;
    } # DESTROY

sub rows ($)
{
    shift->{f_stmt}->{NUM_OF_ROWS};
    } # rows

package DBD::File::Statement;

use strict;
use Carp;

# We may have a working flock () built-in but that doesn't mean that locking
# will work on NFS (flock () may hang hard)
my $locking = eval { flock STDOUT, 0; 1 };

# Jochen's old check for flock ()
#
# my $locking = $^O ne "MacOS"  &&
#              ($^O ne "MSWin32" || !Win32::IsWin95 ())  &&
#               $^O ne "VMS";

@DBD::File::Statement::ISA = qw( DBI::SQL::Nano::Statement );

my $open_table_re = sprintf "(?:%s|%s|%s)",
	quotemeta (File::Spec->curdir  ()),
	quotemeta (File::Spec->updir   ()),
	quotemeta (File::Spec->rootdir ());

sub get_file_name ($$$)
{
    my ($self, $data, $table) = @_;
    my $quoted = 0;
    $table =~ s/^\"// and $quoted = 1;    # handle quoted identifiers
    $table =~ s/\"$//;
    my $file = $table;
    if (    $file !~ m/^$open_table_re/o
	and $file !~ m{^[/\\]}      # root
	and $file !~ m{^[a-z]\:}    # drive letter
	) {
	exists $data->{Database}{f_map}{$table} or
	    DBD::File::file2table ($data->{Database},
		$data->{Database}{f_dir}, $file, 1, $quoted);
	$file = $data->{Database}{f_map}{$table} || undef;
	}
    return ($table, $file);
    } # get_file_name

sub open_table ($$$$$)
{
    my ($self, $data, $table, $createMode, $lockMode) = @_;
    my $file;
    ($table, $file) = $self->get_file_name ($data, $table);
    defined $file && $file ne "" or croak "No filename given";
    require IO::File;
    my $fh;
    my $safe_drop = $self->{ignore_missing_table} ? 1 : 0;
    if ($createMode) {
	-f $file and
	    croak "Cannot create table $table: Already exists";
	$fh = IO::File->new ($file, "a+") or
	    croak "Cannot open $file for writing: $!";
	$fh->seek (0, 0) or
	    croak "Error while seeking back: $!";
	}
    else {
	unless ($fh = IO::File->new ($file, ($lockMode ? "r+" : "r"))) {
	    $safe_drop or croak "Cannot open $file: $!";
	    }
	}
    $fh and binmode $fh;
    if ($locking and $fh) {
	if ($lockMode) {
	    flock $fh, 2 or croak "Cannot obtain exclusive lock on $file: $!";
	    }
	else {
	    flock $fh, 1 or croak "Cannot obtain shared lock on $file: $!";
	    }
	}
    my $columns = {};
    my $array   = [];
    my $pos     = $fh ? $fh->tell () : undef;
    my $tbl     = {
	file          => $file,
	fh            => $fh,
	col_nums      => $columns,
	col_names     => $array,
	first_row_pos => $pos,
	};
    my $class = ref $self;
    $class =~ s/::Statement/::Table/;
    bless $tbl, $class;
    $tbl;
    } # open_table

package DBD::File::Table;

use strict;
use Carp;

@DBD::File::Table::ISA = qw(DBI::SQL::Nano::Table);

sub drop ($)
{
    my $self = shift;
    # We have to close the file before unlinking it: Some OS'es will
    # refuse the unlink otherwise.
    $self->{fh} and $self->{fh}->close ();
    unlink $self->{file};
    return 1;
    } # drop

sub seek ($$$$)
{
    my ($self, $data, $pos, $whence) = @_;
    if ($whence == 0 && $pos == 0) {
	$pos = $self->{first_row_pos};
	}
    elsif ($whence != 2 || $pos != 0) {
	croak "Illegal seek position: pos = $pos, whence = $whence";
	}

    $self->{fh}->seek ($pos, $whence) or
	croak "Error while seeking in " . $self->{file} . ": $!";
    } # seek

sub truncate ($$)
{
    my ($self, $data) = @_;
    $self->{fh}->truncate ($self->{fh}->tell ()) or
	croak "Error while truncating " . $self->{file} . ": $!";
    1;
    } # truncate

1;

__END__

=head1 NAME

DBD::File - Base class for writing DBI drivers

=head1 SYNOPSIS

 This module is a base class for writing other DBDs.
 It is not intended to function as a DBD itself.
 If you want to access flatfiles, use DBD::AnyData, or DBD::CSV,
 (both of which are subclasses of DBD::File).

=head1 DESCRIPTION

The DBD::File module is not a true DBI driver, but an abstract
base class for deriving concrete DBI drivers from it. The implication is,
that these drivers work with plain files, for example CSV files or
INI files. The module is based on the SQL::Statement module, a simple
SQL engine.

See L<DBI> for details on DBI, L<SQL::Statement> for details on
SQL::Statement and L<DBD::CSV> or L<DBD::IniFile> for example
drivers.


=head2 Metadata

The following attributes are handled by DBI itself and not by DBD::File,
thus they all work like expected:

    Active
    ActiveKids
    CachedKids
    CompatMode             (Not used)
    InactiveDestroy
    Kids
    PrintError
    RaiseError
    Warn                   (Not used)

The following DBI attributes are handled by DBD::File:

=over 4

=item AutoCommit

Always on

=item ChopBlanks

Works

=item NUM_OF_FIELDS

Valid after C<$sth->execute>

=item NUM_OF_PARAMS

Valid after C<$sth->prepare>

=item NAME

Valid after C<$sth->execute>; undef for Non-Select statements.

=item NULLABLE

Not really working, always returns an array ref of one's, as DBD::CSV
doesn't verify input data. Valid after C<$sth->execute>; undef for
Non-Select statements.

=back

These attributes and methods are not supported:

    bind_param_inout
    CursorName
    LongReadLen
    LongTruncOk

Additional to the DBI attributes, you can use the following dbh
attribute:

=over 4

=item f_dir

This attribute is used for setting the directory where CSV files are
opened. Usually you set it in the dbh, it defaults to the current
directory ("."). However, it is overwritable in the statement handles.

=item f_ext

This attribute is used for setting the file extension where (CSV) files are
opened. There are several possibilities.

    DBI:CSV:f_dir=data;f_ext=.csv

In this case, DBD::File will open only C<table.csv> if both C<table.csv> and
C<table> exist in the datadir. The table will still be named C<table>. If
your datadir has files with extensions, and you do not pass this attribute,
your table is named C<table.csv>, which is probably not what you wanted. The
extension is always case-insensitive. The table names are not.

    DBI:CSV:f_dir=data;f_ext=.csv/r

In this case the extension is required, and all filenames that do not match
are ignored.

=item f_schema

This will set the schema name. Default is the owner of the folder in which
the table file resides.  C<undef> is allowed.

    my $dbh = DBI->connect ("dbi:CSV:", "", "", {
        f_schema => undef,
        f_dir    => "data",
        f_ext    => ".csv/r",
        }) or die $DBI::errstr;

The effect is that when you get table names from DBI, you can force all
tables into the same (or no) schema:

    my @tables $dbh->tables ();

    # no f_schema
    "merijn".foo
    "merijn".bar

    # f_schema => "dbi"
    "dbi".foo
    "dbi".bar

    # f_schema => undef
    foo
    bar

=back

=head2 Driver private methods

=over 4

=item data_sources

The C<data_sources> method returns a list of subdirectories of the current
directory in the form "DBI:CSV:f_dir=$dirname".

If you want to read the subdirectories of another directory, use

    my ($drh) = DBI->install_driver ("CSV");
    my (@list) = $drh->data_sources (f_dir => "/usr/local/csv_data" );

=item list_tables

This method returns a list of file names inside $dbh->{f_dir}.
Example:

    my ($dbh) = DBI->connect ("DBI:CSV:f_dir=/usr/local/csv_data");
    my (@list) = $dbh->func ("list_tables");

Note that the list includes all files contained in the directory, even
those that have non-valid table names, from the view of SQL.

=back

=head1 KNOWN BUGS

=over 8

=item *

The module is using flock () internally. However, this function is not
available on all platforms. Using flock () is disabled on MacOS and
Windows 95: There's no locking at all (perhaps not so important on
MacOS and Windows 95, as there's a single user anyways).

=back

=head1 AUTHOR

This module is currently maintained by

H.Merijn Brand < h.m.brand at xs4all.nl > and
Jens Rehsack  < rehsack at googlemail.com >

The original author is Jochen Wiedmann.

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2009 by H.Merijn Brand & Jens Rehsack
Copyright (C) 2004 by Jeff Zucker
Copyright (C) 1998 by Jochen Wiedmann

All rights reserved.

You may freely distribute and/or modify this module under the terms of
either the GNU General Public License (GPL) or the Artistic License, as
specified in the Perl README file.

=head1 SEE ALSO

L<DBI>, L<Text::CSV>, L<Text::CSV_XS>, L<SQL::Statement>

=cut
