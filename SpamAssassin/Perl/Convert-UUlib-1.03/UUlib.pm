package Convert::UUlib;

use Carp;

require Exporter;
require DynaLoader;

$VERSION = "1.03";

@ISA = qw(Exporter DynaLoader);

@_consts = qw(
	ACT_COPYING ACT_DECODING ACT_ENCODING ACT_IDLE ACT_SCANNING

	FILE_DECODED FILE_ERROR FILE_MISPART FILE_NOBEGIN FILE_NODATA
	FILE_NOEND FILE_OK FILE_READ FILE_TMPFILE

	MSG_ERROR MSG_FATAL MSG_MESSAGE MSG_NOTE MSG_PANIC MSG_WARNING

	OPT_BRACKPOL OPT_DEBUG OPT_DESPERATE OPT_DUMBNESS OPT_ENCEXT
	OPT_ERRNO OPT_FAST OPT_IGNMODE OPT_IGNREPLY OPT_OVERWRITE OPT_PREAMB
	OPT_PROGRESS OPT_SAVEPATH OPT_TINYB64 OPT_USETEXT OPT_VERBOSE
	OPT_VERSION OPT_REMOVE OPT_MOREMIME OPT_DOTDOT

	RET_CANCEL RET_CONT RET_EXISTS RET_ILLVAL RET_IOERR RET_NODATA
	RET_NOEND RET_NOMEM RET_OK RET_UNSUP

	B64_ENCODED BH_ENCODED PT_ENCODED QP_ENCODED
	XX_ENCODED UU_ENCODED YENC_ENCODED
);

@_funcs = qw(
        Initialize CleanUp GetOption SetOption strerror SetMsgCallback
        SetBusyCallback SetFileCallback SetFNameFilter SetFileNameCallback
        FNameFilter LoadFile GetFileListItem RenameFile DecodeToTemp
        RemoveTemp DecodeFile InfoFile Smerge QuickDecode EncodeMulti
        EncodePartial EncodeToStream EncodeToFile E_PrepSingle
        E_PrepPartial

        straction strencoding strmsglevel
);

@EXPORT = @_consts;
@EXPORT_OK = @_funcs;
%EXPORT_TAGS = (all => [@_consts,@_funcs], constants => \@_consts);

bootstrap Convert::UUlib $VERSION;

Initialize();

# not when < 5.005_6x
# END { CleanUp() }

for (@_consts) {
   my $constant = constant($_);
   *$_ = sub () { $constant };
}

# action code -> string mapping
sub straction($) {
   return 'copying'	if $_[0] == &ACT_COPYING;
   return 'decoding'	if $_[0] == &ACT_DECODING;
   return 'encoding'	if $_[0] == &ACT_ENCODING;
   return 'idle'	if $_[0] == &ACT_IDLE;
   return 'scanning'	if $_[0] == &ACT_SCANNING;
   'unknown';
}

# encoding type -> string mapping
sub strencoding($) {
   return 'uuencode'		if $_[0] == &UU_ENCODED;
   return 'base64'		if $_[0] == &B64_ENCODED;
   return 'yenc'		if $_[0] == &YENC_ENCODED;
   return 'binhex'		if $_[0] == &BH_ENCODED;
   return 'plaintext'		if $_[0] == &PT_ENCODED;
   return 'quoted-printable'	if $_[0] == &QP_ENCODED;
   return 'xxencode'		if $_[0] == &XX_ENCODED;
   'unknown';
}

sub strmsglevel($) {
   return 'message'	if $_[0] == &MSG_MESSAGE;
   return 'note'	if $_[0] == &MSG_NOTE;
   return 'warning'	if $_[0] == &MSG_WARNING;
   return 'error'	if $_[0] == &MSG_ERROR;
   return 'panic'	if $_[0] == &MSG_PANIC;
   return 'fatal'	if $_[0] == &MSG_FATAL;
   'unknown';
}

1;
__END__

=head1 NAME

Convert::UUlib - Perl interface to the uulib library (a.k.a. uudeview/uuenview).

=head1 SYNOPSIS

 use Convert::UUlib ':all';
 
 # read all the files named on the commandline and decode them
 # into the CURRENT directory. See below for a longer example.
 LoadFile $_ for @ARGV;
 for (my $i = 0; my $uu = GetFileListItem $i; $i++) {
    if ($uu->state & FILE_OK) {
      $uu->decode;
      print $uu->filename, "\n";
    }
 }

=head1 DESCRIPTION

Read the file doc/library.pdf from the distribution for in-depth
information about the C-library used in this interface, and the rest of
this document and especially the non-trivial decoder program at the end.

=head1 EXPORTED CONSTANTS

=head2 Action code constants

  ACT_IDLE      we don't do anything
  ACT_SCANNING  scanning an input file
  ACT_DECODING  decoding into a temp file
  ACT_COPYING   copying temp to target
  ACT_ENCODING  encoding a file

=head2 Message severity levels

  MSG_MESSAGE   just a message, nothing important
  MSG_NOTE      something that should be noticed
  MSG_WARNING   important msg, processing continues
  MSG_ERROR     processing has been terminated
  MSG_FATAL     decoder cannot process further requests
  MSG_PANIC     recovery impossible, app must terminate

=head2 Options

  OPT_VERSION	version number MAJOR.MINORplPATCH (ro)
  OPT_FAST	assumes only one part per file
  OPT_DUMBNESS	switch off the program's intelligence
  OPT_BRACKPOL	give numbers in [] higher precendence
  OPT_VERBOSE	generate informative messages
  OPT_DESPERATE	try to decode incomplete files
  OPT_IGNREPLY	ignore RE:plies (off by default)
  OPT_OVERWRITE	whether it's OK to overwrite ex. files
  OPT_SAVEPATH	prefix to save-files on disk
  OPT_IGNMODE	ignore the original file mode
  OPT_DEBUG	print messages with FILE/LINE info
  OPT_ERRNO	get last error code for RET_IOERR (ro)
  OPT_PROGRESS	retrieve progress information
  OPT_USETEXT	handle text messages
  OPT_PREAMB	handle Mime preambles/epilogues
  OPT_TINYB64	detect short B64 outside of Mime
  OPT_ENCEXT	extension for single-part encoded files
  OPT_REMOVE	remove input files after decoding (dangerous)
  OPT_MOREMIME	strict MIME adherence
  OPT_DOTDOT	".."-unescaping has not yet been done on input files

=head2 Result/Error codes

  RET_OK        everything went fine
  RET_IOERR     I/O Error - examine errno
  RET_NOMEM     not enough memory
  RET_ILLVAL    illegal value for operation
  RET_NODATA    decoder didn't find any data
  RET_NOEND     encoded data wasn't ended properly
  RET_UNSUP     unsupported function (encoding)
  RET_EXISTS    file exists (decoding)
  RET_CONT      continue -- special from ScanPart
  RET_CANCEL    operation canceled

=head2 File States

 This code is zero, i.e. "false":

  UUFILE_READ   Read in, but not further processed

 The following state codes are or'ed together:

  FILE_MISPART  Missing Part(s) detected
  FILE_NOBEGIN  No 'begin' found
  FILE_NOEND    No 'end' found
  FILE_NODATA   File does not contain valid uudata
  FILE_OK       All Parts found, ready to decode
  FILE_ERROR    Error while decoding
  FILE_DECODED  Successfully decoded
  FILE_TMPFILE  Temporary decoded file exists

=head2 Encoding types

  UU_ENCODED    UUencoded data
  B64_ENCODED   Mime-Base64 data
  XX_ENCODED    XXencoded data
  BH_ENCODED    Binhex encoded
  PT_ENCODED    Plain-Text encoded (MIME)
  QP_ENCODED    Quoted-Printable (MIME)
  YENC_ENCODED  yEnc encoded (non-MIME)

=head1 EXPORTED FUNCTIONS

=head2 Initializing and cleanup

Initialize is automatically called when the module is loaded and allocates
quite a small amount of memory for todays machines ;) CleanUp releases that
again.

On my machine, a fairly complete decode with DBI backend needs about 10MB
RSS to decode 20000 files.

=over 4

=item Initialize

Not normally necessary, (re-)initializes the library.

=item CleanUp

Not normally necessary, could be called at the end to release memory
before starting a new decoding round.

=back

=head2 Setting and querying options

=over 4

=item $option = GetOption OPT_xxx

=item SetOption OPT_xxx, opt-value

=back

See the C<OPT_xxx> constants above to see which options exist.

=head2 Setting various callbacks

=over 4

=item SetMsgCallback [callback-function]

=item SetBusyCallback [callback-function]

=item SetFileCallback [callback-function]

=item SetFNameFilter [callback-function]

=back

=head2 Call the currently selected FNameFilter

=over 4

=item $file = FNameFilter $file

=back

=head2 Loading sourcefiles, optionally fuzzy merge and start decoding

=over 4

=item ($retval, $count) = LoadFile $fname, [$id, [$delflag, [$partno]]]

Load the given file and scan it for encoded contents. Optionally tag it
with the given id, and if C<$delflag> is true, delete the file after it
is no longer necessary. If you are certain of the part number, you can
specify it as the last argument.

A better (usually faster) way of doing this is using the C<SetFNameFilter>
functionality.

=item $retval = Smerge $pass

If you are desperate, try to call C<Smerge> with increasing C<$pass>
values, beginning at C<0>, to try to merge parts that usually would not
have been merged.

Most probably this will result in garbled files, so never do this by
default.

=item $item = GetFileListItem $item_number

Return the C<$item> structure for the C<$item_number>'th found file, or
C<undef> of no file with that number exists.

The first file has number C<0>, and the series has no holes, so you can
iterate over all files by starting with zero and incrementing until you
hit C<undef>.

=back

=head2 Decoding files

=over 4

=item $retval = $item->rename($newname)

Change the ondisk filename where the decoded file will be saved.

=item $retval = $item->decode_temp

Decode the file into a temporary location, use C<< $item->infile >> to
retrieve the temporary filename.

=item $retval = $item->remove_temp

Remove the temporarily decoded file again.

=item $retval = $item->decode([$target_path])

Decode the file to it's destination, or the given target path.

=item $retval = $item->info(callback-function)

=back

=head2 Querying (and setting) item attributes

=over 4

=item $state    = $item->state

=item $mode     = $item->mode([newmode])

=item $uudet    = $item->uudet

=item $size     = $item->size

=item $filename = $item->filename([newfilename})

=item $subfname = $item->subfname

=item $mimeid   = $item->mimeid

=item $mimetype = $item->mimetype

=item $binfile  = $item->binfile

=back

=head2 Information about source parts

=over 4

=item $parts = $item->parts

Return information about all parts (source files) used to decode the file
as a list of hashrefs with the following structure:

 {
   partno   => <integer describing the part number, starting with 1>,
   # the following member sonly exist when they contain useful information
   sfname   => <local pathname of the file where this part is from>,
   filename => <the ondisk filename of the decoded file>,
   subfname => <used to cluster postings, possibly the posting filename>,
   subject  => <the subject of the posting/mail>,
   origin   => <the possible source (From) address>,
   mimetype => <the possible mimetype of the decoded file>,
   mimeid   => <the id part of the Content-Type>,
 }

Usually you are interested mostly the C<sfname> and possibly the C<partno>
and C<filename> members.

=back

=head2 Functions below not documented and not very well tested

  QuickDecode
  EncodeMulti
  EncodePartial
  EncodeToStream
  EncodeToFile
  E_PrepSingle
  E_PrepPartial

=head2 EXTENSION FUNCTIONS

Functions found in this module but not documented in the uulib documentation:

=over 4

=item $msg = straction ACT_xxx

Return a human readable string representing the given action code.

=item $msg = strerror RET_xxx

Return a human readable string representing the given error code.

=item $str = strencoding xxx_ENCODED

Return the name of the encoding type as a string.

=item $str = strmsglevel MSG_xxx

Returns the message level as a string.

=item SetFileNameCallback $cb

Sets (or queries) the FileNameCallback, which is called whenever the
decoding library can't find a filename and wants to extract a filename
from the subject line of a posting. The callback will be called with
two arguments, the subject line and the current candidate for the
filename. The latter argument can be C<undef>, which means that no
filename could be found (and likely no one exists, so it is safe to also
return C<undef> in this case). If it doesn't return anything (not even
C<undef>!), then nothing happens, so this is a no-op callback:

   sub cb {
      return ();
   }

If it returns C<undef>, then this indicates that no filename could be
found. In all other cases, the return value is taken to be the filename.

This is a slightly more useful callback:

  sub cb {
     return unless $_[1]; # skip "Re:"-plies et al.
     my ($subject, $filename) = @_;
     # if we find some *.rar, take it
     return $1 if $subject =~ /(\w+\.rar)/;
     # otherwise just pass what we have
     return ();
  }

=back

=head1 LARGE EXAMPLE DECODER

This is the file C<example-decoder> from the distribution, put here
instead of more thorough documentation.

 # decode all the files in the directory uusrc/ and copy
 # the resulting files to uudst/

 use Convert::UUlib ':all';

 sub namefilter {
    my($path)=@_;
    $path=~s/^.*[\/\\]//;
    $path;
 }

 sub busycb {
    my ($action, $curfile, $partno, $numparts, $percent, $fsize) = @_;
    $_[0]=straction($action);
    print "busy_callback(", (join ",",@_), ")\n";
    0;
 }

 SetOption OPT_IGNMODE, 1;
 SetOption OPT_VERBOSE, 1;

 # show the three ways you can set callback functions. I normally
 # prefer the one with the sub inplace.
 SetFNameFilter \&namefilter;

 SetBusyCallback "busycb", 333;

 SetMsgCallback sub {
    my ($msg, $level) = @_;
    print uc strmsglevel $_[1], ": $msg\n";
 };

 # the following non-trivial FileNameCallback takes care
 # of some subject lines not detected properly by uulib:
 SetFileNameCallback sub {
    return unless $_[1]; # skip "Re:"-plies et al.
    local $_ = $_[0];

    # the following rules are rather effective on some newsgroups,
    # like alt.binaries.games.anime, where non-mime, uuencoded data
    # is very common

    # if we find some *.rar, take it as the filename
    return $1 if /(\S{3,}\.(?:[rstuvwxyz]\d\d|rar))\s/i;

    # one common subject format
    return $1 if /- "(.{2,}?\..+?)" (?:yenc )?\(\d+\/\d+\)/i;

    # - filename.par (04/55)
    return $1 if /- "?(\S{3,}\.\S+?)"? (?:yenc )?\(\d+\/\d+\)/i;

    # - (xxx) No. 1 sayuri81.jpg 756565 bytes
    # - (20 files) No.17 Roseanne.jpg [2/2]
    return $1 if /No\.[ 0-9]+ (\S+\....) (?:\d+ bytes )?\[/;

    # otherwise just pass what we have
    return ();
 };

 # now read all files in the directory uusrc/*
 for(<uusrc/*>) {
    my($retval,$count)=LoadFile ($_, $_, 1);
    print "file($_), status(", strerror $retval, ") parts($count)\n";
 }

 SetOption OPT_SAVEPATH, "uudst/";

 # now wade through all files and their source parts
 $i = 0;
 while ($uu = GetFileListItem($i)) {
    $i++;
    print "file nr. $i";
    print " state ", $uu->state;
    print " mode ", $uu->mode;
    print " uudet ", strencoding $uu->uudet;
    print " size ", $uu->size;
    print " filename ", $uu->filename;
    print " subfname ", $uu->subfname;
    print " mimeid ", $uu->mimeid;
    print " mimetype ", $uu->mimetype;
    print "\n";

    # print additional info about all parts
    for ($uu->parts) {
       while (my ($k, $v) = each %$_) {
          print "$k > $v, ";
       }
       print "\n";
    }

    $uu->decode_temp;
    print " temporarily decoded to ", $uu->binfile, "\n";
    $uu->remove_temp;

    print strerror $uu->decode;
    print " saved as uudst/", $uu->filename, "\n";
 }

 print "cleanup...\n";

 CleanUp();

=head1 AUTHOR

Marc Lehmann <pcg@goof.com>, the original uulib library was written
by Frank Pilhofer <fp@informatik.uni-frankfurt.de>, and later heavily
bugfixed by Marc Lehmann.

=head1 SEE ALSO

perl(1), uudeview homepage at http://www.uni-frankfurt.de/~fp/uudeview/.

=cut
