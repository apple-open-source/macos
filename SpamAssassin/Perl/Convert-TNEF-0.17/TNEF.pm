# Convert::TNEF.pm
#
# Copyright (c) 1999 Douglas Wilson <dougw@cpan.org>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package Convert::TNEF;

use strict;
use integer;
use vars qw(
  $VERSION
  $TNEF_SIGNATURE
  $TNEF_PURE
  $LVL_MESSAGE
  $LVL_ATTACHMENT
  $errstr
  $g_file_cnt
  %dflts
  %atp
  %att
  %att_name
);

use Carp;
use IO::Wrap;
use File::Spec;
use MIME::Body;

$VERSION = '0.17';

# Set some TNEF constants. Everything turned
# out to be in little endian order, so I just added
# 'reverse' everywhere that I needed to
# instead of reversing the hex codes.
$TNEF_SIGNATURE = reverse pack( 'H*', '223E9F78' );
$TNEF_PURE      = reverse pack( 'H*', '00010000' );

$LVL_MESSAGE    = pack( 'H*', '01' );
$LVL_ATTACHMENT = pack( 'H*', '02' );

%atp = (
  Triples => pack( 'H*', '0000' ),
  String  => pack( 'H*', '0001' ),
  Text    => pack( 'H*', '0002' ),
  Date    => pack( 'H*', '0003' ),
  Short   => pack( 'H*', '0004' ),
  Long    => pack( 'H*', '0005' ),
  Byte    => pack( 'H*', '0006' ),
  Word    => pack( 'H*', '0007' ),
  Dword   => pack( 'H*', '0008' ),
  Max     => pack( 'H*', '0009' ),
);

for ( keys %atp ) {
  $atp{$_} = reverse $atp{$_};
}

sub _ATT {
  my ( $att, $id ) = @_;
  return reverse($id) . $att;
}

# The side comments are 'MAPI' equivalents
%att = (
  Null => _ATT( pack( 'H*', '0000' ), pack( 'H4', '0000' ) ),
  # PR_ORIGINATOR_RETURN_ADDRESS
  From => _ATT( $atp{Triples}, pack( 'H*', '8000' ) ),
  # PR_SUBJECT
  Subject  => _ATT( $atp{String}, pack( 'H*', '8004' ) ),
  # PR_CLIENT_SUBMIT_TIME
  DateSent => _ATT( $atp{Date},   pack( 'H*', '8005' ) ),
  # PR_MESSAGE_DELIVERY_TIME
  DateRecd => _ATT( $atp{Date}, pack( 'H*', '8006' ) ),
  # PR_MESSAGE_FLAGS
  MessageStatus => _ATT( $atp{Byte}, pack( 'H*', '8007' ) ),
  # PR_MESSAGE_CLASS
  MessageClass => _ATT( $atp{Word}, pack( 'H*', '8008' ) ),
  # PR_MESSAGE_ID
  MessageID => _ATT( $atp{String}, pack( 'H*', '8009' ) ),
  # PR_PARENT_ID
  ParentID => _ATT( $atp{String}, pack( 'H*', '800A' ) ),
  # PR_CONVERSATION_ID
  ConversationID => _ATT( $atp{String}, pack( 'H*', '800B' ) ),
  Body     => _ATT( $atp{Text},  pack( 'H*', '800C' ) ),    # PR_BODY
  # PR_IMPORTANCE
  Priority => _ATT( $atp{Short}, pack( 'H*', '800D' ) ),
  # PR_ATTACH_DATA_xxx
  AttachData => _ATT( $atp{Byte}, pack( 'H*', '800F' ) ),
  # PR_ATTACH_FILENAME
  AttachTitle => _ATT( $atp{String}, pack( 'H*', '8010' ) ),
  # PR_ATTACH_RENDERING
  AttachMetaFile => _ATT( $atp{Byte}, pack( 'H*', '8011' ) ),
  # PR_CREATION_TIME
  AttachCreateDate => _ATT( $atp{Date}, pack( 'H*', '8012' ) ),
  # PR_LAST_MODIFICATION_TIME
  AttachModifyDate => _ATT( $atp{Date}, pack( 'H*', '8013' ) ),
  # PR_LAST_MODIFICATION_TIME
  DateModified => _ATT( $atp{Date}, pack( 'H*', '8020' ) ),
  #PR_ATTACH_TRANSPORT_NAME
  AttachTransportFilename => _ATT( $atp{Byte}, pack( 'H*', '9001' ) ),
  AttachRenddata => _ATT( $atp{Byte}, pack( 'H*', '9002' ) ),
  MAPIProps      => _ATT( $atp{Byte}, pack( 'H*', '9003' ) ),
  # PR_MESSAGE_RECIPIENTS
  RecipTable           => _ATT( $atp{Byte}, pack( 'H*', '9004' ) ),
  Attachment           => _ATT( $atp{Byte},  pack( 'H*', '9005' ) ),
  TnefVersion          => _ATT( $atp{Dword}, pack( 'H*', '9006' ) ),
  OemCodepage          => _ATT( $atp{Byte},  pack( 'H*', '9007' ) ),
  # PR_ORIG_MESSAGE_CLASS
  OriginalMessageClass => _ATT( $atp{Word},  pack( 'H*', '0006' ) ),

  # PR_RCVD_REPRESENTING_xxx or PR_SENT_REPRESENTING_xxx
  Owner => _ATT( $atp{Byte}, pack( 'H*', '0000' ) ),
  # PR_SENT_REPRESENTING_xxx
  SentFor => _ATT( $atp{Byte}, pack( 'H*', '0001' ) ),
  # PR_RCVD_REPRESENTING_xxx
  Delegate => _ATT( $atp{Byte}, pack( 'H*', '0002' ) ),
  # PR_DATE_START
  DateStart => _ATT( $atp{Date}, pack( 'H*', '0006' ) ),
  DateEnd  => _ATT( $atp{Date}, pack( 'H*', '0007' ) ),  # PR_DATE_END
  # PR_OWNER_APPT_ID
  AidOwner => _ATT( $atp{Long}, pack( 'H*', '0008' ) ),
  # PR_RESPONSE_REQUESTED
  RequestRes => _ATT( $atp{Short}, pack( 'H*', '0009' ) ),
);

# Create reverse lookup table
%att_name = reverse %att;

# Global counter for creating file names
$g_file_cnt = 0;

# Set some package global defaults for new objects
# which can be overridden for any individual object.
%dflts = (
  debug               => 0,
  debug_max_display   => 1024,
  debug_max_line_size => 64,
  ignore_checksum     => 0,
  display_after_err   => 32,
  output_to_core      => 4096,
  output_dir          => File::Spec->curdir,
  output_prefix       => "tnef",
  buffer_size         => 1024,
);

# Make a file name
sub _mk_fname {
  my $parms = shift;
  File::Spec->catfile( $parms->{output_dir},
    $parms->{output_prefix} . "-" . $$ . "-"
      . ++$g_file_cnt . ".doc" );
}

sub _rtn_err {
  my ( $errmsg, $fh, $parms ) = @_;
  $errstr = $errmsg;
  if ( $parms->{debug} ) {
    my $read_size = $parms->{display_after_err} || 32;
    my $data;
    $fh->read( $data, $read_size );
    print "Error: $errstr\n";
    print "Data:\n";
    print $1, "\n" while $data =~
      /([^\r\n]{0,$parms->{debug_max_line_size}})\r?\n?/g;
    print "HData:\n";
    my $hdata = unpack( "H*", $data );
    print $1, "\n"
      while $hdata =~ /(.{0,$parms->{debug_max_line_size}})/g;
  }
  return undef;
}

sub _read_err {
  my ( $bytes, $fh, $errmsg ) = @_;
  $errstr =
    ( defined $bytes ) ? "Premature EOF" : "Read Error:" . $errmsg;
  return undef;
}

sub read_ent {
  croak "Usage: Convert::TNEF->read_ent(entity, parameters) "
    unless @_ == 2 or @_ == 3;
  my $self = shift;
  my ( $ent, $parms ) = @_;
  my $io = $ent->open("r") or do {
    $errstr = "Can't open entity: $!";
    return undef;
  };
  my $tnef = $self->read( $io, $parms );
  $io->close or do {
    $errstr = "Error closing handle: $!";
    return undef;
  };
  return $tnef;
}

sub read_in {
  croak "Usage: Convert::TNEF->read_in(filename, parameters) "
    unless @_ == 2 or @_ == 3;
  my $self = shift;
  my ( $fname, $parms ) = @_;
  open( INFILE, "<$fname" ) or do {
    $errstr = "Can't open $fname: $!";
    return undef;
  };
  binmode INFILE;
  my $tnef = $self->read( \*INFILE, $parms );
  close INFILE or do {
    $errstr = "Error closing $fname: $!";
    return undef;
  };
  return $tnef;
}

sub read {
  croak "Usage: Convert::TNEF->read(fh, parameters) "
    unless @_ == 2 or @_ == 3;
  my $self = shift;
  my $class = ref($self) || $self;
  $self = {};
  bless $self, $class;
  my ( $fd, $parms ) = @_;
  $fd = wraphandle($fd);

  my %parms = %dflts;
  @parms{ keys %$parms } = values %$parms if defined $parms;
  $parms = \%parms;
  my $debug           = $parms{debug};
  my $ignore_checksum = $parms{ignore_checksum};

  # Start of TNEF stream
  my $data;
  my $num_bytes = $fd->read( $data, 4 );
  return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 4;
  print "TNEF start: ", unpack( "H*", $data ), "\n" if $debug;
  return _rtn_err( "Not TNEF-encapsulated", $fd, $parms )
    unless $data eq $TNEF_SIGNATURE;

  # Key
  $num_bytes = $fd->read( $data, 2 );
  return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 2;
  print "TNEF key: ", unpack( "H*", $data ), "\n" if $debug;

  # Start of First Object
  $num_bytes = $fd->read( $data, 1 );
  return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 1;

  my $msg_att = "";

  my $is_msg = ( $data eq $LVL_MESSAGE );
  my $is_att = ( $data eq $LVL_ATTACHMENT );
  print "TNEF object start: ", unpack( "H*", $data ), "\n" if $debug;
  return _rtn_err( "Neither a message nor an attachment", $fd,
    $parms )
    unless $is_msg or $is_att;

  my $msg = Convert::TNEF::Data->new;
  my @atts;

  # Current message or attachment in loop
  my $ent = $msg;

  # Read message and attachments
  LOOP: {
    my $type = $is_msg ? 'message' : 'attachment';
    print "Reading $type attribute\n" if $debug;
    $num_bytes = $fd->read( $data, 4 );
    return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 4;
    my $att_id   = $data;
    my $att_name = $att_name{$att_id};

    print "TNEF $type attribute: ", unpack( "H*", $data ), "\n"
      if $debug;
    return _rtn_err( "Bad Attribute found in $type", $fd, $parms )
      unless $att_name{$att_id};
    if ( $att_id eq $att{TnefVersion} ) {
      return _rtn_err( "Version attribute found in attachment", $fd,
        $parms )
        if $is_att;
    } elsif ( $att_id eq $att{MessageClass} ) {
      return _rtn_err( "MessageClass attribute found in attachment",
        $fd, $parms )
        if $is_att;
    } elsif ( $att_id eq $att{AttachRenddata} ) {
      return _rtn_err( "AttachRenddata attribute found in message",
        $fd, $parms )
        if $is_msg;
      push @atts, ( $ent = Convert::TNEF::Data->new );
    } else {
      return _rtn_err( "AttachRenddata must be first attribute", $fd,
        $parms )
        if $is_att
        and !@atts
        and $att_name ne "AttachRenddata";
    }
    print "Got attribute:$att_name{$att_id}\n" if $debug;

    $num_bytes = $fd->read( $data, 4 );
    return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 4;

    print "HLength:", unpack( "H8", $data ), "\n" if $debug;
    my $length = unpack( "V", $data );
    print "Length: $length\n" if $debug;

    # Get the attribute data (returns an object since data may
    # actually end up in a file)
    my $calc_chksum;
    $data = _build_data( $fd, $length, \$calc_chksum, $parms )
      or return undef;
    _debug_print( $length, $att_id, $data, $parms ) if $debug;
    $ent->datahandle( $att_name, $data, $length );

    $num_bytes = $fd->read( $data, 2 );
    return _read_err( $num_bytes, $fd, $! ) unless $num_bytes == 2;
    my $file_chksum = $data;
    if ($debug) {
      print "Calc Chksum:", unpack( "H*", $calc_chksum ), "\n";
      print "File Chksum:", unpack( "H*", $file_chksum ), "\n";
    }
    return _rtn_err( "Bad Checksum", $fd, $parms )
      unless $calc_chksum eq $file_chksum
      or $ignore_checksum;

    my $num_bytes = $fd->read( $data, 1 );

    # EOF (0 bytes) is ok
    return _read_err( $num_bytes, $fd, $! ) unless defined $num_bytes;
    last LOOP if $num_bytes < 1;
    print "Next token:", unpack( "H2", $data ), "\n" if $debug;
    $is_msg = ( $data eq $LVL_MESSAGE );
    return _rtn_err( "Found message data in attachment", $fd, $parms )
      if $is_msg and $is_att;
    $is_att = ( $data eq $LVL_ATTACHMENT );
    redo LOOP if $is_msg or $is_att;
    return _rtn_err( "Not a TNEF $type", $fd, $parms );
  }

  print "EOF\n" if $debug;

  $self->{TN_Message}     = $msg;
  $self->{TN_Attachments} = \@atts;
  return $self;
}

sub _debug_print {
  my ( $length, $att_id, $data, $parms ) = @_;
  if ( $length < $parms->{debug_max_display} ) {
    $data = $data->data;
    if ( $att_id eq $att{TnefVersion} ) {
      $data = unpack( "L", $data );
      print "Version: $data\n";
    } elsif ( substr( $att_id, 2 ) eq $atp{Date} and $length == 14 ) {
      my ( $yr, $mo, $day, $hr, $min, $sec, $dow ) =
        unpack( "vvvvvvv", $data );
      my $date = join ":", $yr, $mo, $day, $hr, $min, $sec, $dow;
      print "Date: $date\n";
      print "HDate:", unpack( "H*", $data ), "\n";
    } elsif ( $att_id eq $att{AttachRenddata} and $length == 14 ) {
      my ( $atyp, $ulPosition, $dxWidth, $dyHeight, $dwFlags ) =
        unpack( "vVvvV", $data );
      $data = join ":", $atyp, $ulPosition, $dxWidth, $dyHeight,
        $dwFlags;
      print "AttachRendData: $data\n";
    } else {
      print "Data:\n";
      print $1, "\n" while $data =~
        /([^\r\n]{0,$parms->{debug_max_line_size}})\r?\n?/g;
      print "HData:\n";
      my $hdata = unpack( "H*", $data );
      print $1, "\n"
        while $hdata =~ /(.{0,$parms->{debug_max_line_size}})/g;
    }
  } else {
    my $io = $data->open("r")
      or croak "Error opening attachment data handle: $!";
    my $buffer;
    $io->read( $buffer, $parms->{debug_max_display} );
    $io->close or croak "Error closing attachment data handle: $!";
    print "Data:\n";
    print $1, "\n" while $buffer =~
      /([^\r\n]{0,$parms->{debug_max_line_size}})\r?\n?/sg;
    print "HData:\n";
    my $hdata = unpack( "H*", $buffer );
    print $1, "\n"
      while $hdata =~ /(.{0,$parms->{debug_max_line_size}})/g;
  }
}

sub _build_data {
  my ( $fd, $length, $chksumref, $parms ) = @_;
  my $cutoff = $parms->{output_to_core};
  my $incore = do {
    if    ( $cutoff eq 'NONE' ) { 0 }    #Everything to files
    elsif ( $cutoff eq 'ALL' )  { 1 }    #Everything in memory
    elsif ( $cutoff < $length ) { 0 }    #Large items in files
    else { 1 }                           #Everything else in memory
  };

  # Just borrow some other objects for the attachment attribute data
  my $body =
    ($incore) 
    ? new MIME::Body::Scalar
    : new MIME::Body::File _mk_fname($parms);
  $body->binmode(1);
  my $io     = $body->open("w");
  my $bufsiz = $parms->{buffer_size};
  $bufsiz = $length if $length < $bufsiz;
  my $buffer;
  my $chksum = 0;

  while ( $length > 0 ) {
    my $num_bytes = $fd->read( $buffer, $bufsiz );
    return _read_err( $num_bytes, $fd, $! )
      unless $num_bytes == $bufsiz;
    $io->print($buffer);
    $chksum += unpack( "%16C*", $buffer );
    $chksum %= 65536;
    $length -= $bufsiz;
    $bufsiz = $length if $length < $bufsiz;
  }
  $$chksumref = pack( "v", $chksum );
  $io->close;
  return $body;
}

sub purge {
  my $self = shift;
  my $msg  = $self->{TN_Message};
  my @atts = $self->attachments;
  for ( keys %$msg ) {
    $msg->{$_}->purge if exists $att{$_};
  }
  for my $attch (@atts) {
    for ( keys %$attch ) {
      $attch->{$_}->purge if exists $att{$_};
    }
  }
}

sub message {
  my $self = shift;
  $self->{TN_Message};
}

sub attachments {
  my $self = shift;
  return @{ $self->{TN_Attachments} } if wantarray;
  $self->{TN_Attachments};
}

# This is for Messages or Attachments
# since they are essentially the same thing except
# for the leading attribute code
package Convert::TNEF::Data;

sub new {
  my $proto = shift;
  my $class = ref($proto) || $proto;
  my $self  = {};
  $self->{TN_Size} = {};
  bless $self, $class;
}

sub data {
  my $self = shift;
  my $attr = shift || 'AttachData';
  return $self->{$attr} && $self->{$attr}->as_string;
}

sub name {
  my $self = shift;
  my $attr = shift || 'AttachTitle';
  my $name = $self->{$attr} && $self->{$attr}->data;
  $name =~ s/\x00+$// if $name;
  return $name;
}

# Try to get the long filename out of the
# 'Attachment' attribute.
sub longname {
  my $self = shift;

  my $data = $self->data("Attachment");
  return unless $data;
  my $pos = index( $data, pack( "H*", "1e00013001" ) );
  return $self->name unless $pos >= 0;
  my $len = unpack( "V", substr( $data, $pos + 8, 4 ) );
  my $longname = substr( $data, $pos + 12, $len );
  $longname =~ s/\x00+$// if $longname;
  return $longname || $self->name;
}

sub datahandle {
  my $self = shift;
  my $attr = shift || 'AttachData';
  $self->{$attr} = shift if @_;
  $self->size( $attr, shift ) if @_;
  return $self->{$attr};
}

sub size {
  my $self = shift;
  my $attr = shift || 'AttachData';
  $self->{TN_Size}->{$attr} = shift if @_;
  return $self->{TN_Size}->{$attr};
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__


=head1 NAME

 Convert::TNEF - Perl module to read TNEF files

=head1 SYNOPSIS

 use Convert::TNEF;

 $tnef = Convert::TNEF->read($iohandle, \%parms)
  or die Convert::TNEF::errstr;

 $tnef = Convert::TNEF->read_in($filename, \%parms)
  or die Convert::TNEF::errstr;

 $tnef = Convert::TNEF->read_ent($mime_entity, \%parms)
  or die Convert::TNEF::errstr;

 $tnef->purge;

 $message = $tnef->message;

 @attachments = $tnef->attachments;

 $attribute_value      = $attachments[$i]->data($att_attribute_name);
 $attribute_value_size = $attachments[$i]->size($att_attribute_name);
 $attachment_name = $attachments[$i]->name;
 $long_attachment_name = $attachments[$i]->longname;

 $datahandle = $attachments[$i]->datahandle($att_attribute_name);

=head1 DESCRIPTION

 TNEF stands for Transport Neutral Encapsulation Format, and if you've
 ever been unfortunate enough to receive one of these files as an email
 attachment, you may want to use this module.

 read() takes as its first argument any file handle open
 for reading. The optional second argument is a hash reference
 which contains one or more of the following keys:

=head2

 output_dir - Path for storing TNEF attribute data kept in files
 (default: current directory).

 output_prefix - File prefix for TNEF attribute data kept in files
 (default: 'tnef').

 output_to_core - TNEF attribute data will be saved in core memory unless
 it is greater than this many bytes (default: 4096). May also be set to
 'NONE' to keep all data in files, or 'ALL' to keep all data in core.

 buffer_size - Buffer size for reading in the TNEF file (default: 1024).

 debug - If true, outputs all sorts of info about what the read() function
 is reading, including the raw ascii data along with the data converted
 to hex (default: false).

 display_after_err - If debug is true and an error is encountered,
 reads and displays this many bytes of data following the error
 (default: 32).

 debug_max_display - If debug is true then read and display at most
 this many bytes of data for each TNEF attribute (default: 1024).

 debug_max_line_size - If debug is true then at most this many bytes of
 data will be displayed on each line for each TNEF attribute
 (default: 64).

 ignore_checksum - If true, will ignore checksum errors while parsing
 data (default: false).

 read() returns an object containing the TNEF 'attributes' read from the
 file and the data for those attributes. If all you want are the
 attachments, then this is mostly garbage, but if you're interested then
 you can see all the garbage by turning on debugging. If the garbage
 proves useful to you, then let me know how I can maybe make it more
 useful.

 If an error is encountered, an undefined value is returned and the
 package variable $errstr is set to some helpful message.

 read_in() is a convienient front end for read() which takes a filename
 instead of a handle.

 read_ent() is another convient front end for read() which can take a
 MIME::Entity object (or any object with like methods, specifically
 open("r"), read($buff,$num_bytes), and close ).

 purge() deletes any on-disk data that may be in the attachments of
 the TNEF object.

 message() returns the message portion of the tnef object, if any.
 The thing it returns is like an attachment, but its not an attachment.
 For instance, it more than likely does not have a name or any
 attachment data.

 attachments() returns a list of the attachments that the given TNEF
 object contains. Returns a list ref if not called in array context.

 data() takes a TNEF attribute name, and returns a string value for that 
 attribute for that attachment. Its your own problem if the string is too
 big for memory. If no argument is given, then the 'AttachData' attribute
 is assumed, which is probably the attachment data you're looking for.

 name() is the same as data(), except the attribute 'AttachTitle' is
 the default, which returns the 8 character + 3 character extension name
 of the attachment.

 longname() returns the long filename and extension of an attachment. This
 is embedded within a MAPI property of the 'Attachment' attribute data, so
 we attempt to extract the name out of that.

 size() takes an TNEF attribute name, and returns the size in bytes for
 the data for that attachment attribute.

 datahandle() is a method for attachments which takes a TNEF attribute
 name, and returns the data for that attribute as a handle which is
 the same as a MIME::Body handle.  See MIME::Body for all the applicable
 methods. If no argument is given, then 'AttachData' is assumed.


=head1 EXAMPLES

 # Here's a rather long example where mail is retrieved
 # from a POP3 server based on header information, then
 # it is MIME parsed, and then the TNEF contents
 # are extracted and converted.

 use strict;
 use Net::POP3;
 use MIME::Parser;
 use Convert::TNEF;

 my $mail_dir = "mailout";
 my $mail_prefix = "mail";

 my $pop = new Net::POP3 ( "pop3server_name" );
 my $num_msgs = $pop->login("user_name","password");
 die "Can't login: $!" unless defined $num_msgs;

 # Get mail by sender and subject
 my $mail_out_idx = 0;
 MESSAGE: for ( my $i=1; $i<= $num_msgs;  $i++ ) {
  my $header = join "", @{$pop->top($i)};

  for ($header) {
   next MESSAGE unless
    /^from:.*someone\@somewhere.net/im &&
    /^subject:\s*important stuff/im
  }

  my $fname = $mail_prefix."-".$$.++$mail_out_idx.".doc";
  open (MAILOUT, ">$mail_dir/$fname")
   or die "Can't open $mail_dir/$fname: $!";
  # If the get() complains, you need the new libnet bundle
  $pop->get($i, \*MAILOUT) or die "Can't read mail";
  close MAILOUT or die "Error closing $mail_dir/$fname";
  # If you want to delete the mail on the server
  # $pop->delete($i);
 }

 close MAILOUT;
 $pop->quit();

 # Parse the mail message into separate mime entities
 my $parser=new MIME::Parser;
 $parser->output_dir("mimemail");

 opendir(DIR, $mail_dir) or die "Can't open directory $mail_dir: $!";
 my @files = map { $mail_dir."/".$_ } sort
  grep { -f "$mail_dir/$_" and /$mail_prefix-$$-/o } readdir DIR;
 closedir DIR;

 for my $file ( @files ) {
  my $entity=$parser->parse_in($file) or die "Couldn't parse mail";
  print_tnef_parts($entity);
  # If you want to delete the working files
  # $entity->purge;
 }

 sub print_tnef_parts {
  my $ent = shift;

  if ( $ent->parts ) {
   for my $sub_ent ( $ent->parts ) {
    print_tnef_parts($sub_ent);
   }
  } elsif ( $ent->mime_type =~ /ms-tnef/i ) {

   # Create a tnef object
   my $tnef = Convert::TNEF->read_ent($ent,{output_dir=>"tnefmail"})
    or die $Convert::TNEF::errstr;
   for ($tnef->attachments) {
    print "Title:",$_->name,"\n";
    print "Data:\n",$_->data,"\n";
   }

   # If you want to delete the working files
   # $tnef->purge;
  }
 }

=head1 SEE ALSO

perl(1), IO::Wrap(3), MIME::Parser(3), MIME::Entity(3), MIME::Body(3)

=head1 CAVEATS

 The parsing may depend on the endianness (see perlport) and width of
 integers on the system where the TNEF file was created. If this proves
 to be the case (check the debug output), I'll see what I can do
 about it.

=head1 AUTHOR

 Douglas Wilson, dougw@cpan.org

=cut

