#   -*- perl -*-
#
#
#   PlRPC - Perl RPC, package for writing simple, RPC like clients and
#       servers
#
#
#   Copyright (c) 1997,1998  Jochen Wiedmann
#
#   You may distribute under the terms of either the GNU General Public
#   License or the Artistic License, as specified in the Perl README file.
#
#   Author: Jochen Wiedmann
#           Email: jochen.wiedmann at freenet.de
#

require 5.004;
use strict;

require Storable;


package RPC::PlServer::Comm;


$RPC::PlServer::Comm::VERSION = '0.1003';


############################################################################
#
#   Name:    new (Class method)
#
#   Purpose: Constructor
#
#   Inputs:  $class - This class
#            $attr - Hash ref of attributes
#
#   Result:  Server object for success, error message otherwise
#
############################################################################

sub new ($) {
    my($class, $attr) = @_;
    my $self = {};
    bless($self, (ref($class) || $class));

    if (my $comp = $attr->{'compression'}) {
	if ($comp eq 'off') {
	    $self->{'compression'} = undef;
	} elsif ($comp eq 'gzip') {
	    require Compress::Zlib;
	    $self->{'compression'} = 'gzip';
	} else {
	    die "Unknown compression type ($comp), use 'off' or 'gzip'";
	}
    }
    if (my $cipher = $attr->{'cipher'}) {
	$self->{'cipher'} = $cipher;
    }
    if (my $maxmessage = $attr->{'maxmessage'}) {
	$self->{'maxmessage'} = $maxmessage;
    }

    $self;
}


############################################################################
#
#   Name:    Write
#
#   Purpose: Writing to a PlRPC socket; used by both the client (when
#            sending a method name and arguments) and the server (for
#            sending the result list). Communication occurrs in packets.
#            Each packet is preceeded by 4 bytes with the true packet
#            size. If encryption happens, then the packet is padded with
#            NUL bytes to a multiple of blocksize bytes. However, the
#            stored size remains unchanged.
#
#   Inputs:  $self - Instance of RPC::PlServer or RPC::PlClient
#            $socket - The socket to write to
#            $args - Reference to array of arguments being sent
#
#   Result:  Nothing; dies in case of errors.
#
############################################################################

sub Write ($$$) {
    my($self, $socket, $msg) = @_;

    my $encodedMsg = Storable::nfreeze($msg);
    $encodedMsg = Compress::Zlib::compress($encodedMsg)
	if ($self->{'compression'});

    my($encodedSize) = length($encodedMsg);
    if (my $cipher = $self->{'cipher'}) {
	my $size = $cipher->blocksize;
	if (my $addSize = length($encodedMsg) % $size) {
	    $encodedMsg .= chr(0) x ($size - $addSize);
	}
	$msg = '';
	for (my $i = 0;  $i < length($encodedMsg);  $i += $size) {
	    $msg .= $cipher->encrypt(substr($encodedMsg, $i, $size));
	}
	$encodedMsg = $msg;
    }

    local $\;
    if (!$socket->print(pack("N", $encodedSize), $encodedMsg)  ||
	!$socket->flush()) {
	die "Error while writing socket: $!";
    }
}


############################################################################
#
#   Name:    Read
#
#   Purpose: Reading from a PlRPC socket; used by both the client (when
#            receiving a result list) and the server (for receiving the
#            method name and arguments). Counterpart of Write, see
#            above for specs.
#
#   Inputs:  $self - Instance of RPC::PlServer or RPC::PlClient
#            $socket - The socket to read from
#
#   Result:  Array ref to result list; dies in case of errors.
#
############################################################################

sub Read($$) {
    my($self, $socket) = @_;
    my $result;

    my($encodedSize, $readSize, $blockSize);
    $readSize = 4;
    $encodedSize = '';
    while ($readSize > 0) {
	my $result = $socket->read($encodedSize, $readSize,
				   length($encodedSize));
	if (!$result) {
	    return undef if defined($result);
	    die "Error while reading socket: $!";
	}
	$readSize -= $result;
    }
    $encodedSize = unpack("N", $encodedSize);
    my $max = $self->getMaxMessage();
    die "Maximum message size of $max exceeded, use option 'maxmessage' to"
	. " increase" if $max  &&  $encodedSize > $max;
    $readSize = $encodedSize;
    if ($self->{'cipher'}) {
	$blockSize = $self->{'cipher'}->blocksize;
	if (my $addSize = ($encodedSize % $blockSize)) {
	    $readSize += ($blockSize - $addSize);
	}
    }
    my $msg = '';
    my $rs = $readSize;
    while ($rs > 0) {
	my $result = $socket->read($msg, $rs, length($msg));
	if (!$result) {
	    die "Unexpected EOF" if defined $result;
	    die "Error while reading socket: $!";
	}
	$rs -= $result;
    }
    if ($self->{'cipher'}) {
	my $cipher = $self->{'cipher'};
	my $encodedMsg = $msg;
	$msg = '';
	for (my $i = 0;  $i < $readSize;  $i += $blockSize) {
	    $msg .= $cipher->decrypt(substr($encodedMsg, $i, $blockSize));
	}
	$msg = substr($msg, 0, $encodedSize);
    }
    $msg = Compress::Zlib::uncompress($msg) if ($self->{'compression'});
    Storable::thaw($msg);
}


############################################################################
#
#   Name:    Init
#
#   Purpose: Initialize an object for using RPC::PlServer::Comm methods
#
#   Input:   $self - Instance
#
#   Returns: The instance in case of success, dies in case of trouble.
#
############################################################################

############################################################################
#
#   Name:    getMaxMessage
#
#   Purpose: Returns the maximum size of a message
#
#   Inputs:  None
#
#   Returns: Maximum message size or 65536, if none specified
#
############################################################################

sub getMaxMessage() {
    my $self = shift;
    return defined($self->{'maxmessage'}) ?
	$self->{'maxmessage'} : 65536;
}


1;
