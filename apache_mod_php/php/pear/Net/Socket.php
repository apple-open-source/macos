<?php
//
// +----------------------------------------------------------------------+
// | PHP version 4.0                                                      |
// +----------------------------------------------------------------------+
// | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
// +----------------------------------------------------------------------+
// | This source file is subject to version 2.0 of the PHP license,       |
// | that is bundled with this package in the file LICENSE, and is        |
// | available at through the world-wide-web at                           |
// | http://www.php.net/license/2_02.txt.                                 |
// | If you did not receive a copy of the PHP license and are unable to   |
// | obtain it through the world-wide-web, please send a note to          |
// | license@php.net so we can mail you a copy immediately.               |
// +----------------------------------------------------------------------+
// | Authors: Stig Bakken <ssb@fast.no>                                   |
// |          Chuck Hagenbuch <chuck@horde.org>                           |
// +----------------------------------------------------------------------+
//
// $Id: Socket.php,v 1.1.1.2 2001/01/25 05:00:30 wsanchez Exp $
//

require_once 'PEAR.php';

/**
 * Generalized Socket class. More docs to be written.
 *
 * @version 0.2
 * @author Stig Bakken <ssb@fast.no>
 * @author Chuck Hagenbuch <chuck@horde.org>
 */
class Net_Socket extends PEAR {
    
    // {{{ properties
    
    /** Socket file pointer. */
    var $fp = null;
    
    /** Whether the socket is blocking. */
    var $blocking = true;
    
    /** Whether the socket is persistant. */
    var $persistent;
    
    /** The IP address to connect to. */
    var $addr = '';
    
    /** The port number to connect to. */
    var $port = 0;
    
    /** Number of seconds to wait on socket connections before
        assuming there's no more data. */
    var $timeout = false;
    
    /** Number of bytes to read at a time in readLine() and
        readAll(). */
    var $lineLength = 2048;
	// }}}
    
    
    // {{{ constructor
    /**
     * Constructs a new Net_Socket object.
     *
     * @access public
     */
    function Net_Socket() {
        $this->PEAR();
    }
	// }}}
    
    
	// {{{ connect()
    /**
     * Connect to the specified port. If called when the socket is
     * already connected, it disconnects and connects again.
     *
     * @param $addr string IP address or host name
     * @param $port int TCP port number
     * @param $persistent bool (optional) whether the connection is
     *        persistent (kept open between requests by the web server)
     * @param $timeout int (optional) how long to wait for data
     * @access public
     * @return mixed true on success or error object
     */
    function connect($addr, $port, $persistent = false, $timeout = false) {
        if (is_resource($this->fp)) {
            @fclose($this->fp);
            $this->fp = null;
        }
        
        if (strspn($addr, '.0123456789') == strlen($addr)) {
            $this->addr = $addr;
        } else {
            $this->addr = gethostbyname($addr);
        }
        $this->port = $port % 65536;
        $this->timeout = $timeout;
        $openfunc = $persistent ? 'pfsockopen' : 'fsockopen';
        $errno = 0;
        $errstr = '';
        if ($this->timeout) {
            $fp = $openfunc($this->addr, $this->port, $errno, $errstr, $this->timeout);
        } else {
            $fp = $openfunc($this->addr, $this->port, $errno, $errstr);
        }
        
        if (!$fp) {
            return new PEAR_Error($errstr, $errno);
        }
        
        socket_set_blocking($fp, $this->blocking);
        $this->fp = $fp;
        $this->persistent = $persistent;
        return true;
    }
	// }}}
    
	// {{{ disconnect()
    /**
     * Disconnects from the peer, closes the socket.
     *
     * @access public
     * @return mixed true on success or an error object otherwise
     */
    function disconnect() {
        if (is_resource($this->fp)) {
            fclose($this->fp);
            $this->fp = null;
            return true;
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ isBlocking()
    /**
     * Find out if the socket is in blocking mode.
     *
     * @access public
     * @return bool the current blocking mode.
     */
    function isBlocking() {
        return $this->blocking;
    }
	// }}}
    
	// {{{ setBlocking()
    /**
     * Sets whether the socket connection should be blocking or
     * not. A read call to a non-blocking socket will return immediately
     * if there is no data available, whereas it will block until there
     * is data for blocking sockets.
     *
     * @param $mode bool true for blocking sockets, false for nonblocking
     * @access public
     */
    function setBlocking($mode) {
        $this->blocking = $mode;
        if (is_resource($this->fp)) {
            set_socket_blocking($this->fp, $mode);
        }
    }
	// }}}
    
	// {{{ gets()
    function gets($size) {
        if (is_resource($this->fp)) {
            return fgets($this->fp, $size);
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ read()
    /**
     * Read a specified amount of data. This is guaranteed to return,
     * and has the added benefit of getting everything in one fread()
     * chunk; if you know the size of the data you're getting
     * beforehand, this is definitely the way to go.
     *
     * @param $size The number of bytes to read from the socket.
     * @access public
     * @return $size bytes of data from the socket, or a PEAR_Error if
     *         not connected.
     */
    function read($size) {
        if (is_resource($this->fp)) {
            return fread($this->fp, $size);
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ write()
    function write($data) {
        if (is_resource($this->fp)) {
            return fwrite($this->fp, $data);
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
    // {{{ writeLine()
    /**
     * Write a line of data to the socket, followed by a trailing "\r\n".
     *
     * @access public
     * @return mixed fputs result, or an error
     */
    function writeLine ($data) {
        if (is_resource($this->fp)) {
            return fwrite($this->fp, "$data\r\n");
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ eof()
    function eof() {
        return (is_resource($this->fp) && feof($this->fp));
    }
	// }}}
    
	// {{{ readByte()
    function readByte() {
        if (is_resource($this->fp)) {
            return ord(fread($this->fp, 1));
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ readWord()
    function readWord() {
        if (is_resource($this->fp)) {
            $buf = fread($this->fp, 2);
            return (ord($buf[0]) + (ord($buf[1]) << 8));
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ readInt()
    function readInt() {
        if (is_resource($this->fp)) {
            $buf = fread($this->fp, 4);
            return (ord($buf[0]) + (ord($buf[1]) << 8) +
                    (ord($buf[2]) << 16) + (ord($buf[3]) << 24));
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ readString()
    function readString() {
        if (is_resource($this->fp)) {
            $string = '';
            while (($char = fread($this->fp, 1)) != "\x00")  {
                $string .= $char;
            }
            return $string;
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ readIPAddress()
    function readIPAddress() {
        if (is_resource($this->fp)) {
            $buf = fread($this->fp, 4);
            return sprintf("%s.%s.%s.%s", ord($buf[0]), ord($buf[1]),
                           ord($buf[2]), ord($buf[3]));
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
	// {{{ readLine()
    /**
     * Read until either the end of the socket or a newline, whichever
     * comes first. Strips the trailing newline from the returned data.
     *
     * @access public
     * @return All available data up to a newline, without that
     *         newline, or until the end of the socket.
     */
    function readLine() {
        if (is_resource($this->fp)) {
            $line = '';
            $timeout = time() + $this->timeout;
            while (!feof($this->fp) && (!$this->timeout || time() < $timeout)) {
                $line .= fgets($this->fp, $this->lineLength);
                $len = strlen($line);
                if ($len >=2 && substr($line, $len-2, 2) == "\r\n")
                    return substr($line, 0, $len-2);
            }
            return $line;
        }
        return new PEAR_ERROR("not connected");
    }
	// }}}
    
	// {{{ readAll()
    /**
     * Read until the socket closes. THIS FUNCTION WILL NOT EXIT if the
     * socket is in blocking mode until the socket closes.
     *
     * @access public
     * @return All data until the socket closes.
     */
    function readAll() {
        if (is_resource($this->fp)) {
            $data = '';
            while (!feof($this->fp))
                $data .= fread($this->fp, $this->lineLength);
            return $data;
        }
        return new PEAR_Error("not connected");
    }
	// }}}
    
}

?>
